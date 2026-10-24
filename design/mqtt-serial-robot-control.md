# Feature: MQTT-to-serial robot control (PC -> M5 -> Nexus)

## Goal
The Nexus 4WD 58mm omni-wheel robot (10020) receives control commands from an M5 over serial. The M5 gets those commands over MQTT from a PC server.

PC (MQTT broker + test tool) --MQTT/WiFi--> M5 (bridge) --UART 19200 8N1--> Nexus board (Omni4WD)

## Nexus board (from `design/nexus`)
- Arduino-328-compatible board (ATmega328, 16 MHz, 2 KB SRAM), programmed with the Arduino IDE (board "Arduino Duemilanove or Diecimila").
- Robot library is `MotorWheel` (`Omni4WD`), with `PID_Beta6` and `PinChangeInt` as dependencies.
- The original sketch (`program/RB009_RB020_Omni4WD_PID`) only ran a demo loop and had no host interface.
- Wheel pins (PWM, DIR, encA, encB): wheel1 3,2,4,5; wheel2 11,12,A0,A1; wheel3 9,8,A2,A3; wheel4 10,7,A4,A5.
- Hardware UART is D0/D1 (5 V TTL). The APC220 wireless and RS485 jumpers share it and must be removed.

## Implemented files
| Path | Purpose |
|---|---|
| `nexus/starrobox_nexus/starrobox_nexus.ino` | Serial command parser driving `Omni4WD`, with a 1 s failsafe |
| `m5/starrobox_m5_bridge/` | PlatformIO project; the `mqtt` env (`pio run -e mqtt`) is a WiFi + MQTT client forwarding commands to UART and replies back |
| `m5/starrobox_m5_bridge/src/config.example.h` | WiFi, broker, topic and pin settings (copy to git-ignored `config.h`) |
| `server/robot_ctl.py` | paho-mqtt PC tool: `send`, `monitor`, `keys` and `manual` (live keyboard control over MQTT) |
| `README.md` | Wiring, build steps, protocol |

## Wiring
M5 TX (GPIO17) -> Nexus D0; Nexus D1 -> M5 RX (GPIO16) through a 5 V -> 3.3 V level shifter or divider; common GND.

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
Written but not compiled or run on hardware (no toolchain in the session). Only the Python tool was syntax-checked.

## Assumptions and open items
- M5 model assumed to be M5Stack Core (Serial2 on GPIO16/17); change `platformio.ini` and pins for other models.
- Robot 10020 assumed to use the same pin map as RB009/RB020; verify against the manual.
- `MOVE` angle is counter-clockwise from right (0 = right, 90 = forward, 270 = backward), taken from `Omni4WD.cpp`; confirm on the robot with wheels lifted.
- USB serial debugging is unavailable while the M5 is connected to D0/D1.

## Verification plan
1. Nexus alone: send `PING`, `ADV 150`, `STOP` from a serial terminal at 19200 and check the 1 s failsafe (wheels lifted).
2. M5 bridge: run mosquitto, publish `PING` to `starrobox/robot/cmd`, expect `PONG` on the status topic.
3. End to end: `python server/robot_ctl.py send "MOVE 200 90 0"`, then confirm the robot stops on `STOP` and when the broker or WiFi is cut.
