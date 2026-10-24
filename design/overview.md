# Overview: control modes

The Nexus 4WD omni robot runs one firmware (`nexus/starrobox_nexus/starrobox_nexus.ino`) that accepts line-based ASCII commands on its hardware UART. Only the path that carries those lines changes between modes.

| Mode | Path | Details | PC command |
|---|---|---|---|
| Remote (MQTT) | PC -> MQTT -> M5 -> UART -> Nexus | [mqtt-serial-robot-control.md](mqtt-serial-robot-control.md) | `robot_ctl.py manual`, `send`, `keys` |
| Phone (BLE) | Phone -> BLE -> M5 -> UART -> Nexus | [ble-phone-control.md](ble-phone-control.md) | open `phone/index.html` |
| Direct (USB) | PC -> USB serial -> Nexus | [direct-usb-manual-control.md](direct-usb-manual-control.md) | `robot_ctl.py direct COM5` |

## Switching modes
All modes use the Nexus UART on D0/D1, so only one link can be connected at a time.

**M5 modes: Remote (MQTT) <-> Phone (BLE)**
The wiring is the same; the M5 firmware differs and is chosen at build time.
- Remote: `cd m5/starrobox_m5_bridge && pio run -e mqtt -t upload`
- Phone: `pio run -e ble -t upload`

Only one runs on the M5 at a time (WiFi and BLE together are heavy on an ESP32). Reflash to switch.

**Remote -> Direct**
1. Stop any running `robot_ctl.py` session.
2. Unplug the M5 wires from D0/D1.
3. Connect USB and run `robot_ctl.py direct <COM>`.

**Direct -> Remote**
1. Quit the `direct` session (Esc) and unplug USB from the PC, or at least close the port.
2. Reconnect the M5 wires (M5 TX -> D0, D1 -> M5 RX through a level shifter, common GND).
3. Start the broker and run `robot_ctl.py manual` or `send`.

In both modes the wireless (APC220) and RS485 jumpers stay removed, and the Nexus stops by itself after 1 s without a command.

## Shared
- Protocol and failsafe: see the protocol section of [mqtt-serial-robot-control.md](mqtt-serial-robot-control.md).
- Both modes use the same keyboard loop in `server/robot_ctl.py`; only the send function differs.
