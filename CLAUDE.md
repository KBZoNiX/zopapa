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
- `app/` — Zopapa Control, the Flutter/Android companion app for the BT
  command protocol in `src/zopapa/bt.ino` (start/stop, gain presets,
  preflight, calibration, plus the fuller bench-test/telemetry commands
  behind an Advanced/Practice screen). See `app/README.md` for build steps
  and architecture notes; the protocol it targets is the single source of
  truth in `bt.ino`, not this file or `docs/FSD.html` (which is stale on
  this front — see below).
- `docs/FSD.html` — Functional Specification Document (open in a browser).
- `docs/MANUAL.html` — end-user manual in Spanish (physical controls, BT/
  Serial commands). Keep in sync with `docs/FSD.html` and the DIP/button/
  command tables in this file when firmware HMI behavior changes.
- `docs/MANUAL-TELEMETRIA.html` — companion manual in Spanish for the
  bench-test telemetry/tuning workflow (`O`/`Z`/`N`/`A`/`M` commands added
  for `PLAN-speed-scheduling-v3_1.md` Phase 0/1). Keep in sync with that
  plan and with new telemetry/autotune commands as they land.
- `docs/MANUAL-CALIBRACION.html` — companion manual in Spanish for the
  tournament-day calibration workflow (`F`/`G`/`U` commands and the `C`
  calibration-quality report added for `PLAN-tournament-stopgap-v1.md`).
  Keep in sync with that plan and with `docs/Reglamento-Carreras.md`.
- `docs/Reglamento-Carreras.md` — competition rulebook (Spanish) for the
  league this robot races in. Notably: only phone/BT is allowed once a
  match starts, restricted to loading configs, quick parameter tweaks, and
  profile selection — no PC, no reflashing (§7.4). This is what motivates
  the `tournament-stopgap` branch's gain-preset feature.
- `PLAN-speed-scheduling-v3_1.md` — active investigation into why fixed
  PD gains under-damp at race speed; supersedes the same-named v2/v2.1/v3
  drafts. Phase 2 (decision gate) is not resolved — do not implement
  Phase 3A/3B/3C without checking its results table first.
- `PLAN-tournament-stopgap-v1.md` — pre-competition hardening plan,
  implemented on the `tournament-stopgap` branch off `speed-scheduled-
  autotune`. Deliberately does not implement gain scheduling (Tier 3 of
  that plan) — no `n` has been measured yet. Its Tier 2 items (T-S7 line-
  loss recovery, T-S8 soft start) ship disabled by compile-time `const
  bool` flags in `zopapa.ino` pending on-track validation.

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
- DIP switches (active = LOW, `INPUT_PULLUP`): 1/2/3 (D8/D7/D6) force-load
  gain preset 1/2/3 at boot via `cargarPerfilPorDip()` (`presets.ino`, called
  from `setup()`); none active = compile-time defaults, more than one = no
  load + 4-beep error. DIP 4 (D4) enables the turbine for both the
  pushbutton and BT `S` starts, through `estadoDeArranque()` (`zopapa.ino`).
  There is no DIP-triggered autotune anymore — `T` over BT only.
- `debug_macros.h` is not `#include`d anywhere — currently dead.
- `maxCorrection()`/`relayAmplitude()` in `zopapa.ino` (formerly
  `MAX_CORRECTION`/`RELAY_AMPLITUDE` consts) recompute from `SPEED` on
  every call — fixed 2026-08, previously they froze at the default
  `SPEED=100` forever, ignoring runtime speed changes.
- Sensor calibration (min/max) persists to EEPROM (`guardarCalibracion()`/
  `cargarCalibracion()` in `eeprom.ino`); `setup()` skips the manual
  3-second sweep when valid data is stored, and the `C` serial/BT command
  forces a fresh sweep on demand. Validated on hardware.
- `calibrarRegletaManual()` (`zopapa.ino`) frees and re-nulls
  `Regleta.calibratedMinimum`/`calibratedMaximum` before sweeping. Without
  that, `ATRSensors::calibrate()` only resets those arrays when they're
  `NULL` — otherwise it just widens whatever range was already loaded, so
  a "fresh" recalibration silently became a monotonic union with the old
  one. Confirmed on hardware by comparing two consecutive recalibrations.
- The long-press pushbutton gesture (`zopapa.ino` `loop()`) runs the same
  recalibration as `C`, regardless of DIP position (saving KP/KD/SPEED is BT
  `W` only). It deliberately sets `estado = 1` mid-hold so the post-release
  logic always resolves to Stopped — that's not a bug, it's how the gesture
  guarantees the robot never ends up driving right after a long press,
  regardless of what state it was in beforehand.
- `autotuneRelay()` (`autotune.ino`) always tests at a fixed
  `AUTOTUNE_SPEED_REF` (100), saving and restoring the live `SPEED` around
  the run — added so `T` gives comparable results regardless of whatever
  `SPEED` was set beforehand. All exit paths (edge-abort, both failure
  returns, normal completion) restore `SPEED`; if you add another exit
  path, restore it there too.
- Gain presets (`presets.ino`) are a separate EEPROM region (flag at
  40–41, data at 42–68) from the single manual KP/KD/SPEED slot
  (`eeprom.ino`, addresses 0/4/8/12) and from calibration (1–2, 16–31) —
  see `docs/FSD.html` §10 for the full layout. `seedPresetsIfNeeded()`
  fills it with the §11.1 table on first boot with this layout; `activePreset`
  (`zopapa.ino`, -1 = none) tracks which preset the live KP/KD/SPEED came
  from, and is invalidated by any manual `K`/`D`/`V`/`L`/autotune.
- This build has no gain-scheduling code at all (`PLAN-speed-scheduling-
  v3_1.md` Phase 3A is not implemented) — `V` never touches KP/KD. Don't
  assume a `gainSchedulingActive` flag exists; grep before referencing it.
- `btProcessCommand()`'s `S`/`P`/`O` cases (`bt.ino`) call `indicar_estado()`
  after replying — added so BT-triggered start/stop get the same LED/beep
  feedback as the physical pushbutton, which previously only fired from the
  button's own code path in `zopapa.ino`'s `loop()`.

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
