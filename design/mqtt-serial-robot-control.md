# Feature: MQTT-to-serial robot control (PC -> M5 -> Nexus)

## Goal
The Nexus 4WD 58mm omni-wheel robot (10020) receives control commands from an M5 over serial. The M5 gets those commands over MQTT from a PC server.

PC (MQTT broker + test tool) --MQTT/WiFi--> M5 (bridge) --UART 19200 8N1--> Nexus board (Omni4WD)

## Nexus board (from `design/nexus`)
- Arduino-328-compatible board (ATmega328, 16 MHz, 2 KB SRAM), programmed with the Arduino IDE (board "Arduino Duemilanove or Diecimila").
- Robot library is `MotorWheel` (`Omni4WD`), with `PID_Beta6` and `PinChangeInt` as dependencies.
- The original sketch (`program/RB009_RB020_Omni4WD_PID`) only ran a demo loop and had no host interface.
- Wheel pins (PWM, DIR, encA, encB): wheel1 3,2,4,5; wheel2 11,12,A0,A1; wheel3 9,8,A2,A3; wheel4 10,7,A4,A5.
- The IO Expansion Shield (22006) puts an RS485 transceiver on the hardware UART (D0/D1), with direction control on pin 13 (HIGH = transmit, LOW = receive, as in the `SONAR` library). The M5 link uses this RS485 port; its jumper must be fitted to run and removed to upload or use USB.

## Implemented files
| Path | Purpose |
|---|---|
| `nexus/starrobox_nexus/starrobox_nexus.ino` | Serial command parser driving `Omni4WD`, with a 1 s failsafe |
| `m5/starrobox_m5_bridge/` | PlatformIO project; the `mqtt` env (`pio run -e mqtt`) is a WiFi + MQTT client forwarding commands to UART and replies back |
| `m5/starrobox_m5_bridge/src/config.example.h` | WiFi, broker, topic and pin settings (copy to git-ignored `config.h`) |
| `server/robot_ctl.py` | paho-mqtt PC tool: `send`, `monitor`, `keys` and `manual` (live keyboard control over MQTT) |
| `README.md` | Wiring, build steps, protocol |

## Wiring
AtomS3 RX G5 / TX G6 -> Atomic RS485 Base (auto-direction); base A -> shield A, base B -> shield B (swap them if nothing answers); common GND. The sketch drives pin 13 high only while it sends a reply, after a 5 ms turnaround pause: without it the reply arrives while the base is still switching to receive and is lost. The M5 forwards only `PONG`/`OK`/`ERR…` lines and waits 10 ms between writes to avoid collisions.

## Protocol
One ASCII line per command, `\n` terminated, identical over MQTT and serial.

| Command | Effect |
|---|---|
| `MOVE <mm/s> <angleDeg> <omega>` | `setCarMove` |
| `ADV/BACK/LEFT/RIGHT <mm/s>` | translate |
| `ROTL/ROTR <mm/s>` | rotate |
| `STOP` | stop |
| `PING` | reply `PONG` |

Replies are `OK` or `ERR <reason>`. Speed is clamped to 0-600 mm/s. The robot stops if no command arrives for 1 s.

MQTT topics: `starrobox/robot/cmd` (PC -> M5) and `starrobox/robot/status` (M5 -> PC, includes online/offline).

## Status
Working on hardware: PC -> MQTT broker -> AtomS3 -> RS485 -> Nexus answers `PING`, `STOP` and error cases. Wheel motion through this full path is the remaining test.

## Assumptions and open items
- M5 is an AtomS3 on an Atomic RS485 Base (RX G5, TX G6), as in `design/m5/M5Atom`; the PlatformIO env targets `m5stack-atoms3`.
- Robot 10020 assumed to use the same pin map as RB009/RB020; verify against the manual.
- `MOVE` angle is counter-clockwise from right (0 = right, 90 = forward, 270 = backward), taken from `Omni4WD.cpp`; confirm on the robot with wheels lifted.
- USB serial debugging is unavailable while the RS485 jumper is fitted (the UART is routed to RS485).
- Verified on hardware: PC -> MQTT -> AtomS3 -> RS485 -> Nexus with `PING`/`PONG` and error replies; wheel motion over this path is not yet tested. `mqtt-debug` env and `nexus/rs485_probe` exist for wiring debug.

## Verification plan
1. Nexus alone: send `PING`, `ADV 150`, `STOP` from a serial terminal at 19200 and check the 1 s failsafe (wheels lifted).
2. M5 bridge: run mosquitto, publish `PING` to `starrobox/robot/cmd`, expect `PONG` on the status topic.
3. End to end: `python server/robot_ctl.py send "MOVE 200 90 0"`, then confirm the robot stops on `STOP` and when the broker or WiFi is cut.
