# CLAUDE.md

Guidance for Claude Code when working in this repo.

## Project
Zopapa — a two-wheel differential-drive line-following robot. Arduino Nano
firmware plus two custom KiCad PCBs (main controller, 8-channel sensor
array). Full functional spec: docs/FSD.html.

## Repo layout
- `src/zopapa/` — Arduino sketch. `zopapa.ino` is the entry point; all
  `.ino` files in the folder compile together as one translation unit.
- `src/libraries/` — vendored, first-party libraries (`DRV8833` motor
  driver, `ATRSensors` line sensor reader) — not the SparkFun/Pololu
  libraries of similar names.
- `hardware/main_pcb/` — Rapiduino main PCB, KiCad project.
- `hardware/line_sensor_pcb/` — ATR-8A sensor PCB, KiCad project.
- `docs/FSD.html` — Functional Specification Document (open in a browser).

## Build
```
arduino-cli compile --fqbn arduino:avr:nano --libraries src/libraries src/zopapa
```
Arduino IDE: open `src/zopapa/zopapa.ino` directly. Copy or symlink
`src/libraries/*` into your sketchbook's `libraries/` folder first — the
IDE does not read `--libraries`-style paths.

## Flash
The physical board is a Nano clone with the **old bootloader** (57600 baud),
not the 115200-baud bootloader `arduino:avr:nano` defaults to — the default
FQBN uploads but fails with repeated `not in sync: resp=0x00` errors. Use
the `atmega328old` CPU variant instead:
```
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old --libraries src/libraries -u -p /dev/ttyUSB0 src/zopapa
```
Port is typically `/dev/ttyUSB0` (a generic USB-serial adapter — `arduino-cli
board list` reports it as board type "Unknown", which is expected and not an
error). Requires the user's account to be in the `dialout` group.

## Key facts (see docs/FSD.html for full detail)
- Steering is P+D only (no integral term) despite being called "PID"
  throughout the code.
- The `DRV8833` library drives the main PCB's TB6612FNG module with 2
  Arduino pins per motor in SLOW_DECAY (locked-antiphase) mode; the
  module's STBY/PWMA/PWMB lines are hardwired to +5V on the PCB.
- `ATRSensors` is a speed-optimized, analog-only fork of Pololu's
  QTRSensors (100 units/sensor scale, not 1000) — the ADC prescaler and
  single-sample read are what make an 8-sensor pass fit inside the 2ms
  control loop.
- Serial and Bluetooth share one UART (9600 baud); R5/R6 on the main PCB
  level-shift the Nano's 5V TX line to ~3.3V for the BT module's RX.
- DIP switches 1 & 2 (D8/D7 — "increase speed" / "increase KP/KD") are
  wired and initialized but never read anywhere in the firmware —
  currently dead.
- `debug_macros.h` is not `#include`d anywhere — currently dead.
- `maxCorrection()`/`relayAmplitude()` in `zopapa.ino` (formerly
  `MAX_CORRECTION`/`RELAY_AMPLITUDE` consts) recompute from `SPEED` on
  every call — fixed 2026-08, previously they froze at the default
  `SPEED=100` forever, ignoring runtime speed changes.
- Sensor calibration (min/max) persists to EEPROM (`guardarCalibracion()`/
  `cargarCalibracion()` in `eeprom.ino`); `setup()` skips the manual
  3-second sweep when valid data is stored, and the `C` serial/BT command
  forces a fresh sweep on demand. Validated on hardware.

## Conventions
- Firmware comments and Serial output strings are in Spanish; keep new
  comments consistent with that unless told otherwise.
- EEPROM validity flags: use a multi-byte magic marker, not a single byte
  compared to a small constant like `1`. Confirmed on real hardware that a
  fresh/reused chip can already hold that exact byte from an unrelated
  prior sketch, making a single-byte check falsely report "valid" — see
  the calibration flag in `eeprom.ino` for the pattern to copy.
- No automated test suite exists; verify firmware changes by flashing to
  the physical board and reading Serial output (see Flash above) — don't
  claim a behavioral fix works from compilation alone.
