# Feature: Phone control over BLE (phone -> M5 -> Nexus)

## Goal
Drive the robot from a phone with no WiFi, router or broker. The Nexus board has no wireless of its own, so the M5 acts as a BLE-to-UART bridge.

Phone --BLE (Nordic UART Service)--> M5 (bridge, BLE mode) --UART 19200 8N1--> Nexus board (Omni4WD)

BLE and MQTT are separate M5 modes, chosen at build time. See [overview.md](overview.md).

## Why BLE
- Works on iOS and Android; classic Bluetooth serial (SPP) does not work on iPhones.
- Pair straight to the M5, nothing to configure on a network.
- The Nexus sketch is unchanged: the M5 forwards the same line commands (see the protocol in [mqtt-serial-robot-control.md](mqtt-serial-robot-control.md)).

## Implemented files
| Path | Purpose |
|---|---|
| `m5/starrobox_m5_bridge/src/main.cpp` | `MODE_BLE` build: advertises as `StarroBox`, forwards writes to the UART, notifies Nexus replies |
| `m5/starrobox_m5_bridge/platformio.ini` | `[env:ble]` and `[env:mqtt]`, one per mode |
| `phone/index.html` | Web Bluetooth page: joystick, rotate buttons, speed slider, STOP |

## BLE service
Nordic UART Service, so generic BLE terminal apps (nRF Toolbox, Serial Bluetooth Terminal) also work.

| UUID | Role |
|---|---|
| `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | service |
| `6E400002-...` | RX: phone writes commands (with or without a trailing newline) |
| `6E400003-...` | TX: M5 notifies Nexus replies |

## Safety
- The M5 sends `STOP` when the phone disconnects, then re-advertises.
- The web page resends the active command every 150 ms and sends `STOP` on release.
- The Nexus stops by itself after 1 s without a command.

## Usage
1. `cd m5/starrobox_m5_bridge`, copy `src/config.example.h` to `src/config.h`, then `pio run -e ble -t upload`.
2. Host `phone/index.html` over HTTPS (Web Bluetooth does not work from `file://` or plain http, except on localhost), for example GitHub Pages. Open it in Chrome on Android, or in a Web Bluetooth browser such as Bluefy on iOS.
3. Tap Connect, pick `StarroBox`, and drive with the joystick.

Joystick angles follow the Nexus library: counter-clockwise from right, so 0 = right, 90 = forward, 180 = left, 270 = backward (from `setCarRight` / `setCarBackoff` in `Omni4WD.cpp`).

## Status
The `ble` env compiles for the AtomS3 (M5Unified, `espressif32@6.4.0`). Not yet tested with a phone.

## Open items
- The platform is pinned to `espressif32@6.4.0` (Arduino-ESP32 2.x); the BLE API differs in 3.x.
- Default BLE MTU is 20 bytes, enough for the longest command (`MOVE 600 359 100`).
- No pairing PIN: anyone within range can connect. Add bonding if that matters.

## Verification
1. Flash the `ble` env, open a BLE terminal app, connect to `StarroBox`, write `PING`, expect `PONG`.
2. With wheels lifted, use the web page joystick; release and confirm the wheels stop.
3. Turn off phone Bluetooth mid-motion; confirm the robot stops.
