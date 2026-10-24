# StarroBox robot control

PC --MQTT--> M5 (bridge) --UART 19200 8N1--> Nexus 4WD omni robot.

## Wiring
M5 TX (GPIO17) -> Nexus D0 (RX); Nexus D1 (TX) -> M5 RX (GPIO16) **through a 5V -> 3.3V level shifter or divider**; common GND.
Remove the APC220 wireless and RS485 jumpers on the Nexus (they share the UART). USB serial is
unusable for debugging while the M5 is attached.

## Nexus
Copy `design/nexus/lib/{MotorWheel,PID_Beta6,PinChangeInt}` (plus the stock EEPROM) into the Arduino
libraries folder. Open `nexus/starrobox_nexus/starrobox_nexus.ino`, choose board "Arduino Duemilanove or
Diecimila (ATmega328)" and upload with the jumpers removed. Test with a serial terminal at 19200.

## M5
`cd m5/starrobox_m5_bridge`, copy `src/config.example.h` to `src/config.h`, edit, then pick one mode:
- `pio run -e mqtt -t upload` for PC -> MQTT -> M5 -> Nexus
- `pio run -e ble -t upload` for phone -> BLE -> M5 -> Nexus

The modes are separate; reflash to switch. The default board is M5Stack Core; change `platformio.ini` for other models.

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
The M5 shares the Nexus UART, so disconnect its wires from D0/D1 first, and keep the wireless/RS485
jumpers removed. The board resets when the port opens, so the tool waits 2 s before sending.

## Protocol (one line per command, `\n` terminated; same text over MQTT and serial)
| Command | Effect |
|---|---|
| `MOVE <mm/s> <angleDeg> <omega>` | `setCarMove` |
| `ADV/BACK/LEFT/RIGHT <mm/s>` | translate |
| `ROTL/ROTR <mm/s>` | rotate |
| `STOP` | stop |
| `PING` | reply `PONG` |

Replies are `OK` or `ERR <reason>`. Speed is clamped to 0-600. The robot stops by itself if no command
arrives for 1 s, so resend commands to keep moving. Topics: `starrobox/robot/cmd`, `starrobox/robot/status`.
