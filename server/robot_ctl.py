"""Test tool: publish robot commands over MQTT and watch replies.

  python robot_ctl.py send "MOVE 200 0 0"
  python robot_ctl.py monitor
  python robot_ctl.py manual   # live keyboard control, no Enter needed (Windows)
  python robot_ctl.py direct COM5   # same keyboard control, straight over USB to the Nexus (needs pyserial)
  python robot_ctl.py keys   # type w/a/s/d/q/e + Enter to move, anything else stops
"""
import argparse
import sys
import time

import paho.mqtt.client as mqtt

CMD = "starrobox/robot/cmd"
STATUS = "starrobox/robot/status"
KEYS = {"w": "ADV", "s": "BACK", "a": "LEFT", "d": "RIGHT", "q": "ROTL", "e": "ROTR"}


def connect(host, port):
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    c.connect(host, port)
    c.loop_start()
    return c


HOLD_S = 0.6    # a key press keeps the command alive this long (covers the key-repeat delay)
RESEND_S = 0.2  # must stay well under the Nexus 1 s failsafe


def manual(send, speed):
    """Live keyboard control (Windows console): no Enter needed.

    w/s/a/d move, q/e rotate, space stops, +/- change speed, Esc quits.
    The command is resent while a key is held and STOP is sent shortly after release.
    `send(line)` delivers one command line, over MQTT or straight to the serial port.
    """
    try:
        import msvcrt
    except ImportError:
        sys.exit("manual mode needs a Windows console; use 'keys' instead")

    print("w/a/s/d move, q/e rotate, space stop, +/- speed, Esc quit")
    cmd, until, last_sent, stopped = None, 0.0, 0.0, True
    try:
        while True:
            now = time.monotonic()
            while msvcrt.kbhit():
                ch = msvcrt.getwch().lower()
                if ch == "\x1b":
                    return
                if ch in KEYS:
                    cmd, until = KEYS[ch], now + HOLD_S
                elif ch == " ":
                    cmd, until = None, 0.0
                elif ch in "+=":
                    speed = min(speed + 25, 600)
                    print(f"\rspeed {speed}   ", end="")
                elif ch == "-":
                    speed = max(speed - 25, 25)
                    print(f"\rspeed {speed}   ", end="")
            if cmd and now < until:
                if now - last_sent >= RESEND_S:
                    send(f"{cmd} {speed}")
                    last_sent, stopped = now, False
            elif not stopped:
                send("STOP")
                stopped = True
            time.sleep(0.02)
    finally:
        send("STOP")


def direct(port, baud, speed):
    """Manual control straight over the PC's USB cable to the Nexus board (no M5, no MQTT).

    The M5 shares the Nexus UART, so unplug its wires from D0/D1 first. The Nexus
    wireless/RS485 jumpers must be removed too.
    """
    try:
        import serial
    except ImportError:
        sys.exit("direct mode needs pyserial: pip install pyserial")

    ser = serial.Serial(port, baud, timeout=0.05)
    time.sleep(2)  # the board resets when the port opens

    def send(line):
        ser.write(line.encode() + b"\n")
        while ser.in_waiting:
            reply = ser.readline().decode(errors="replace").strip()
            if reply and reply != "OK":
                print("\r<-", reply)

    try:
        manual(send, speed)
    finally:
        ser.close()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="localhost")
    p.add_argument("--port", type=int, default=1883)
    sub = p.add_subparsers(dest="mode", required=True)
    s = sub.add_parser("send")
    s.add_argument("command")
    sub.add_parser("monitor")
    k = sub.add_parser("keys")
    k.add_argument("--speed", type=int, default=150)
    m = sub.add_parser("manual")
    m.add_argument("--speed", type=int, default=150)
    d = sub.add_parser("direct")
    d.add_argument("com", help="serial port of the Nexus USB link, e.g. COM5")
    d.add_argument("--baud", type=int, default=19200)
    d.add_argument("--speed", type=int, default=150)
    a = p.parse_args()

    if a.mode == "direct":
        direct(a.com, a.baud, a.speed)
        return

    c = connect(a.host, a.port)
    if a.mode == "manual":
        c.on_message = lambda cl, u, m: print("\r<-", m.payload.decode(errors="replace"))
        c.subscribe(STATUS)
        try:
            manual(lambda line: c.publish(CMD, line).wait_for_publish(), a.speed)
        finally:
            c.disconnect()
    elif a.mode == "send":
        c.publish(CMD, a.command).wait_for_publish()
    elif a.mode == "monitor":
        c.on_message = lambda cl, u, m: print(m.payload.decode(errors="replace"))
        c.subscribe(STATUS)
        while True:
            time.sleep(1)
    else:
        c.on_message = lambda cl, u, m: print("<-", m.payload.decode(errors="replace"))
        c.subscribe(STATUS)
        try:
            for ch in sys.stdin:
                ch = ch.strip().lower()[:1]
                if ch in KEYS:
                    # the robot stops after 1 s without commands
                    c.publish(CMD, f"{KEYS[ch]} {a.speed}")
                else:
                    c.publish(CMD, "STOP")
        except KeyboardInterrupt:
            c.publish(CMD, "STOP").wait_for_publish()


if __name__ == "__main__":
    main()
