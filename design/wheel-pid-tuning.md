# Feature: Wheel speed control tuning

## Symptoms
- Wheels commanded to the same speed turned at different speeds.
- After `STOP` the wheels turned forward and backward a little before settling.

## What the Nexus manual says
Nothing numeric. The PID chapter only says "there isn't one right answer ... try several tuning values". The example sketches use `PIDEnable(0.31, 0.01, 0, 10)` for Omni4WD (what this project started with) and `0.26, 0.01-0.02, 0, 10` for R2WD. The library default is `KC 0.31, TAUI 0.02, TAUD 0, SAMPLETIME 5`. The values were therefore left as they are and the causes below were found by measurement.

## Measurements (wheels lifted, `nexus/pid_tune` + `server/pid_tune.py`, motor RPM, wheel order UL LL LR UR)
| Test | Result |
|---|---|
| 150 mm/s, original code | 1741 / 1638 / 1670 / 1753 RPM, spread 6.8%, PWM 255 on all wheels (target 1834 RPM) |
| 100 mm/s | 1210 / 1223 / 1211 / 1224 RPM, spread 1.1% (target 1222 RPM) |
| 490 Hz vs 31 kHz PWM | no difference |
| `STOP` at 150 mm/s, original | speed swung by about +-700 RPM and pulses reversed before settling |

## Causes and fixes (all in `nexus/starrobox_nexus/starrobox_nexus.ino`, no library change)
1. **Saturation.** Above about 135 mm/s (library units) the slowest wheel is at full PWM and cannot reach the setpoint, so each wheel runs at its own open-loop top speed. Fix: `MAX_SPEED_MMPS` is now 120, applied to the wheel speeds (`MOVE` diagonals and rotation are scaled down so no wheel exceeds it, `limitScale`/`clampRot`). The phone slider and `robot_ctl.py` stop at 120.
2. **Reversal on stop.** A stopped wheel sends no encoder pulses, so the library keeps its last speed; the PID then sees a large error and drives the accumulator negative, reversing the motor. Fix: a wheel with no pulse for 60 ms reads 0 RPM, the PID output is never allowed below 0, and `STOP` clears the accumulator and holds PWM 0 until the next move command (`stopWheels`, `startWheels`).
3. **Skipped wheels.** `Omni4WD::PIDRegulate()` is `a && b && c && d`, so wheels 2-4 are skipped whenever wheel 1 is not due. Fix: each wheel is regulated on its own in `regulate()`.

## Open items
- Units: the library assumes a 314 mm wheel circumference (100 mm wheel); this robot has 58 mm wheels (about 182 mm), and the encoder reading is also taken at library CPR 12. "mm/s" is therefore nominal, not real distance per second. Measure the real speed on the floor before using it for distance.
- The 120 limit comes from a 1640 RPM slowest wheel on this battery. A weaker battery lowers it.
- A stop now coasts: about 50 encoder pulses (about 12 mm) at 100 mm/s. Add a short ramp-down in `stopWheels` if that is too much.
- `PIDEnable(0.31, 0.01, 0, 10)` was kept: at 100 mm/s the wheels agree within 1.1% with a steady spread of 3-10 RPM.

## Tools
`nexus/pid_tune/pid_tune.ino` (USB, RS485 jumper removed) streams per-wheel RPM, PWM and pulses and accepts `GAIN`, `CIR`, `FIX`, `PWMF`, `RUN`, `STOP`; `server/pid_tune.py COM13 --speed 100 --cases "A:FIX 31;GAIN 0.31 0.01 0 10|B:..."` runs the step tests.
