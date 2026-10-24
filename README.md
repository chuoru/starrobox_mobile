# StarroBox robot control

PC --MQTT--> M5 (bridge) --UART 19200 8N1--> Nexus 4WD omni robot.

## Wiring
The M5 talks to the Nexus over RS485, not directly over TTL serial:
- M5 side: M5 AtomS3 on an Atomic RS485 Base (auto-direction), using RX G5 and TX G6. A/B polarity matters: if nothing answers, swap A and B.
- Nexus side: the IO Expansion Shield (22006). Its RS485 connector shares the hardware UART and pin 13 sets the direction.
- Connect A to A and B to B between the unit and the shield, plus common GND.
- Jumpers on the shield: **fit** the RS485 jumper when running with the M5; **remove** it (and the wireless jumper) to
  upload a sketch or use the USB cable. USB and RS485 cannot be used at the same time.

## Nexus
Copy `design/nexus/lib/{MotorWheel,PID_Beta6,PinChangeInt}` (plus the stock EEPROM) into the Arduino
libraries folder. Open `nexus/starrobox_nexus/starrobox_nexus.ino`, choose board "Arduino Duemilanove or
Diecimila (ATmega328)" and upload with the RS485 and wireless jumpers removed. Test with a serial terminal at 19200,
then fit the RS485 jumper to run from the M5.

## M5
`cd m5/starrobox_m5_bridge`, copy `src/config.example.h` to `src/config.h`, edit, then pick one mode:
- `pio run -e mqtt -t upload` for PC -> MQTT -> M5 -> Nexus
- `pio run -e ble -t upload` for phone -> BLE -> M5 -> Nexus

The modes are separate; reflash to switch. The board is the M5 AtomS3 (settings follow `design/m5/M5Atom`). Pass
`--upload-port COMxx` to pick the serial port.

Wiring debug: `pio run -e mqtt-debug` also publishes every raw byte received from the RS485 bus to
`starrobox/robot/status/rx`, `pio run -e ble-debug` logs BLE writes, bus bytes and notifications on the M5's USB serial port, and `nexus/rs485_probe` is a small Nexus sketch that sends `HELLO` once a second.

## Phone (BLE mode)
Host `phone/index.html` over HTTPS (Web Bluetooth needs it), open it in Chrome on Android (Bluefy on iOS),
tap Connect and choose `StarroBox`. Generic BLE terminal apps also work (Nordic UART Service). See `design/ble-phone-control.md`.

## PC
Run a broker (`mosquitto`), `pip install paho-mqtt`, then use `server/robot_ctl.py`.

Manual control from the PC: `python server/robot_ctl.py --host <broker> manual`. Hold w/a/s/d to move,
q/e to rotate, space to stop, +/- to change speed, Esc to quit. The robot stops shortly after you release
a key, and on exit.

Direct control, PC to robot over USB (no M5, no MQTT): `pip install pyserial`, then
`python server/robot_ctl.py direct COM5` (use the COM port of the Nexus USB link; same keys as above).
The USB link and RS485 share the Nexus UART, so remove the shield's RS485 jumper (and the wireless jumper) first and
disconnect the M5 side. The board resets when the port opens, so the tool waits 2 s before sending.

## Protocol (one line per command, `\n` terminated; same text over MQTT and serial)
| Command | Effect |
|---|---|
| `MOVE <mm/s> <angleDeg> <omega>` | `setCarMove` |
| `ADV/BACK/LEFT/RIGHT <mm/s>` | translate |
| `ROTL/ROTR <mm/s>` | rotate |
| `STOP` | stop |
| `PING` | reply `PONG` |
| `TURN <ms>` | RS485 reply pause, 0-200 ms (default 30) |

Replies are `OK` or `ERR <reason>`. Speed is clamped to 0-600. The robot stops by itself if no command
arrives for 1 s, so resend commands to keep moving. Topics: `starrobox/robot/cmd`, `starrobox/robot/status`.
