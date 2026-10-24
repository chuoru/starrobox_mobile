# Feature: Magnetic line tracking, RFID seat localization, table-edge (anti-fall) protection

## Goal
The robot drives on a table top, follows a magnetic tape route, knows which seat it is at from an RFID tag, and never drives off a table edge. Parts and procurement links are in `design/bom-nav-sensors.md`.

## Status
Design only. No firmware or hardware has been changed. Pin findings below come from `nexus/starrobox_nexus/starrobox_nexus.ino` and the Nexus *Robot Kits Manual_006.pdf* (pp.1-22); nothing has been tested on the robot yet.

## Can the Nexus board read the sensors?
The Nexus ATmega328 has no spare pins except D6.

| Pins | Used by |
|---|---|
| D0, D1 | UART to the RS485 transceiver |
| D2-D5, D7-D12 | motor PWM/DIR and encoders A/B of wheels 1-4 |
| D13 | RS485 direction |
| A0-A5 | encoders of wheels 2-4 (A4/A5 are also the I2C pins, so the I2C port is unusable) |
| **D6** | **free** |

The IO Expansion Shield breaks out D0-D13 and analog headers 0-5 only, so A6/A7 are not available. The motor-control pin jumpers (pins 4-7 on the controller) tie D6 to the on-board motor driver when fitted. They must be off; D4, D5 and D7 are already used as encoder/DIR pins, so they should be. The manual shows a "Fall Detector" digital-read example (p.18).

| Feature | Done on the Nexus? | Why |
|---|---|---|
| Anti-fall | **Yes**, one digital input on D6 | the signal is a single fail-safe line; the stop must not wait for RS485/BLE (30-170 ms) |
| Magnetic line | No | 8+ channels needed; no free I2C/SPI/UART |
| RFID | No | needs I2C, SPI or UART |

So the M5 AtomS3 (free Grove I2C on G1/G2) does tracking and RFID, and the Nexus does the safety stop.

## Architecture
```
 Hall array F/R -- I2C expander --+
                                   +-- Grove I2C --> M5 AtomS3 -- RS485 --> Nexus (ATmega328) --> 4 wheels
 RFID2 unit (0x28) ---------------+     (navigation)   MOVE / PING             ^
                                                                               | D6 (low = stop)
 4 corner IR cliff sensors -- 74HC21 AND gate ---------------------------------+
```
- The M5 reads the sensors, computes steering and sends `MOVE` every ~100 ms (the Nexus stops by itself after 1 s without a command).
- The Nexus owns the cliff stop. It never depends on the M5 or the link.

## Magnetic line tracking
- Sensors: 8 omnipolar digital Hall switches (DRV5032 family) at about 10 mm pitch on a PCB, read through an I2C expander (MCP23017 or PCF8574, distinct addresses for front and rear boards). Mount front and rear under the chassis, 3-5 mm above the tape.
- Position error is the weighted centroid of active sensors (centre = 0). The omni wheels let the robot correct sideways (`LEFT/RIGHT`) while still moving forward, so it does not need to rotate to stay on the line. The difference between front and rear error gives heading, corrected with a small `omega` in `MOVE`.
- All sensors active = crossing or stop mark. No sensor active for more than a set time = line lost, send `STOP`.
- Tape: pick one width and pole pattern from one supplier (Makome warns that look-alike tape has unstable coercivity). Check Hall trip threshold against the tape field at 3-5 mm before ordering 16 sensors; the AJ variant needs about 9.5 mT, the FB variant (4.8 mT) is obsolete.

## RFID seat localization
- Reader: M5Stack Unit RFID2 (WS1850S, I2C 0x28, read range under 20 mm) on the same Grove bus.
- Mount under the chassis 5-10 mm above the table, centred on the stop position, with a ferrite sheet between the reader and the metal chassis.
- NTAG213 stickers on the table at each seat stop and at home. The M5 holds a UID -> seat ID table. On a known UID at a stop mark the M5 sends `STOP`, then reports `SEAT <id>`.
- Place tags next to, not under, the magnetic tape so they do not disturb the Hall sensors.

## Anti-fall (table edge)
- Four downward IR reflective sensors (TCRT5000 modules with digital output) at the chassis corners. The robot is omni-directional, so any side can reach an edge.
- Each output is HIGH while a surface is seen. The four outputs go through one 4-input AND gate (74HC21) to D6. D6 LOW means stop. A missing surface, a cut wire or a dead sensor supply all read as LOW, so a fault stops the robot.
- Firmware (later step): pin-change interrupt on D6 (the encoders already use PinChangeInt) calls `stopWheels()` and sets a latch. While latched, `MOVE` is refused unless it points away from the edge; `CLEAR` resets. The loop must also poll D6 as a fallback if the interrupt conflicts with the encoder ISRs.
- Placement: mount each sensor at least 25 mm outboard of the wheel contact patch. The coast distance of about 12 mm at 100 mm/s is quoted in library units (see `design/wheel-pid-tuning.md`); real speed is about 0.58 of that (58 mm wheel vs the library's 100 mm), but measure the real stop distance on a table edge before fixing the offset.
- Backup: two mechanical edge whiskers or a skid lip, not relying on optics alone.
- Limits: IR reflective sensors fail on black, glass or very glossy tables. If the table is one of those, use VL53L0X time-of-flight sensors instead (each needs I2C, so they would go on the M5 and would lose the local-stop property; a better fix is then a small extra MCU on D6).
- Test: tune each module's potentiometer on the real table; test every edge approach (forward, sideways, diagonal) at 20, 60 and 120 mm/s with a catch tray.

## Protocol additions (future firmware)
| Command / message | Direction | Meaning |
|---|---|---|
| `PING` reply `PONG CLIFF=<hex mask>` | Nexus -> M5 | current state of D6 (and latch) |
| `CLEAR` | M5 -> Nexus | clear the cliff latch |
| `SEAT <id>`, `LINE <err>` | M5 -> PC/phone | seat reached, line error |

`isNexusReply()` in `m5/starrobox_m5_bridge/src/main.cpp` forwards only `PONG`, `OK` and `ERR` lines, so it must accept the new reply. The Nexus replies only to commands today, so the M5 should poll with `PING` while moving.

## Power
Motor rail 7-12 V (maximum 14 V). Sensors run on the Nexus 5 V/3.3 V outputs; check the current budget (16 Hall sensors are tiny, four TCRT5000 modules about 20 mA each, RFID2 about 30 mA). Add a 5 V buck if the AtomS3 is not powered by USB.

## Open items
- Table material, size and edge shape (decides IR vs ToF).
- Chassis dimensions, payload and battery voltage (not in the repo).
- Whether the D6 jumper is off. Check: connect a switch to D6 and print `digitalRead(6)`.
- Tape width, polarity and supplier; final Hall part.
- Seat count, layout and the route (loop, branches).
- Real mm/s of the robot on the table.

## Verification
1. D6 test sketch with a switch (see Open items).
2. Hall array on the bench with a strip of the real tape at 3-5 mm: every sensor triggers and the centroid is monotonic across the tape.
3. RFID2: read a tag through the ferrite sheet at the chassis height, with the motors running.
4. Cliff: power off one sensor and unplug one wire; the robot must stop in both cases.
