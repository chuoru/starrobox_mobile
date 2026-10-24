# Feature: Direct USB manual control (PC -> Nexus)

## Goal
Drive the robot by keyboard straight from the PC over the Nexus USB cable, with no M5 and no MQTT broker.

PC (`robot_ctl.py direct`) --USB serial 19200 8N1--> Nexus board (Omni4WD)

## Implemented files
| Path | Purpose |
|---|---|
| `server/robot_ctl.py` (`direct <COM>`) | pyserial keyboard control; shares the `manual` key loop with the MQTT mode |
| `nexus/starrobox_nexus/starrobox_nexus.ino` | Same sketch and protocol as the MQTT mode, unchanged |

## Usage
```
pip install pyserial
python server/robot_ctl.py direct COM5 [--baud 19200] [--speed 150]
```
Keys: `w`/`a`/`s`/`d` move, `q`/`e` rotate, space stops, `+`/`-` change speed, Esc quits. The command is resent every 0.2 s while a key is held and `STOP` is sent shortly after release and on exit. Replies other than `OK` are printed.

## Constraints
- The USB link and the M5 share the Nexus UART (D0/D1). Disconnect the M5 wires before using this mode.
- Keep the wireless (APC220) and RS485 jumpers removed.
- The board resets when the port opens, so the tool waits 2 s before sending.
- Windows console only (uses `msvcrt`).
- The Nexus 1 s failsafe stops the robot if the cable drops or the tool dies.

## Status
Written, syntax-checked only; not run against a board.

## Verification
1. With wheels lifted, run `direct COM5`, press `w`, confirm the wheels spin and stop shortly after release.
2. Close the tool or unplug USB mid-motion and confirm the robot stops within about 1 s.
