"""Run step tests against nexus/pid_tune over USB and report wheel speed spread and stop behaviour.

  python server/pid_tune.py COM13 --speed 150 --cases "A:FIX 0;GAIN 0.31 0.01 0 10" ...
Each case is NAME:cmd;cmd;... applied before the run. Wheels must be lifted, motor power on.
"""
import argparse
import statistics
import sys
import time

import serial


def send(ser, cmd):
    ser.write((cmd + "\n").encode())
    ser.flush()
    t = time.time()
    while time.time() - t < 2:
        if ser.readline().strip() in (b"OK", b"ERR"):
            return


def collect(ser, seconds):
    rows = []
    end = time.time() + seconds
    while time.time() < end:
        parts = ser.readline().decode(errors="ignore").split()
        if len(parts) == 14 and parts[0] == "D":
            rows.append([int(x) for x in parts[1:]])
    return rows


def run_case(ser, name, setup, speed, run_s, stop_s):
    send(ser, "STOP")
    send(ser, "QUIET")
    time.sleep(0.7)
    ser.reset_input_buffer()
    for cmd in setup:
        send(ser, cmd)
    send(ser, "RUN %d" % speed)
    run = collect(ser, run_s)
    send(ser, "STOP")
    stop = collect(ser, stop_s)
    send(ser, "QUIET")

    steady = [r for r in run if r[0] > run_s * 1000 * 0.5]
    means = [statistics.mean(abs(r[1 + i]) for r in steady) for i in range(4)]
    sds = [statistics.pstdev(abs(r[1 + i]) for r in steady) for i in range(4)]
    spread = (max(means) - min(means)) / statistics.mean(means) * 100
    # pulses at stop vs the extreme and final pulses after it: how far each wheel kept moving or came back
    p_stop = [run[-1][9 + i] for i in range(4)] if run else [0] * 4
    p_end = [stop[-1][9 + i] for i in range(4)] if stop else [0] * 4
    p_ext = [max(abs(r[9 + i] - p_stop[i]) for r in stop) for i in range(4)] if stop else [0] * 4
    reverse = [
        max((p_stop[i] - r[9 + i]) * (1 if p_stop[i] >= run[0][9 + i] else -1) for r in stop) if stop else 0
        for i in range(4)
    ]
    print("== %s ==" % name)
    print("  mean rpm  :", " ".join("%7.0f" % m for m in means), " spread %.1f%%" % spread)
    print("  rpm sd    :", " ".join("%7.0f" % s for s in sds))
    print("  coast+back:", " ".join("%7d" % e for e in p_ext), " (pulses moved after STOP, max)")
    print("  backwards :", " ".join("%7d" % r for r in reverse), " (pulses reversed past the stop point)")
    print("  final off :", " ".join("%7d" % (p_end[i] - p_stop[i]) for i in range(4)), " (net pulses after STOP)")
    return means


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--speed", type=int, default=150)
    ap.add_argument("--run", type=float, default=3.0)
    ap.add_argument("--stop", type=float, default=1.5)
    ap.add_argument("--cases", required=True, help='"NAME:cmd;cmd" separated by "|"')
    a = ap.parse_args()
    ser = serial.Serial(a.port, 115200, timeout=0.3, write_timeout=2)
    time.sleep(2.2)
    ser.reset_input_buffer()
    try:
        for case in a.cases.split("|"):
            name, _, cmds = case.partition(":")
            run_case(ser, name.strip(), [c.strip() for c in cmds.split(";") if c.strip()], a.speed, a.run, a.stop)
    finally:
        try:
            send(ser, "STOP")
        except serial.SerialException:
            pass
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
