# Zopapa

A two-wheel differential-drive line-following robot: Arduino Nano firmware, an 8-channel reflectance sensor array, and two custom KiCad PCBs.

Full functional specification: [`docs/FSD.html`](docs/FSD.html)

## Features

- PD line-following control loop (proportional + derivative, ~500 Hz)
- Relay-feedback (Åström–Hägglund) autotune — computes and installs new gains from an induced oscillation
- EEPROM persistence of tuned speed/gains
- Serial/Bluetooth command interface for live tuning and telemetry (shared UART, 9600 baud)
- Optional turbine/vacuum subsystem for extra downforce, ramped proportionally to steering correction
- On-board HMI: pushbutton, 4-position DIP bank, status LED, buzzer

## Repo layout

```
src/zopapa/       Arduino sketch (zopapa.ino is the entry point)
src/libraries/    Vendored first-party libraries: DRV8833 (motor driver), ATRSensors (line sensor)
hardware/         KiCad PCB projects — main_pcb (Rapiduino) and line_sensor_pcb (ATR-8A)
docs/FSD.html     Functional Specification Document
```

## Build

Requires [arduino-cli](https://arduino.github.io/arduino-cli/) with the `arduino:avr` core installed.

```sh
arduino-cli compile --fqbn arduino:avr:nano --libraries src/libraries src/zopapa
```

Arduino IDE: open `src/zopapa/zopapa.ino` directly, and copy or symlink `src/libraries/*` into your sketchbook's `libraries/` folder first.

## Flash

Most Nano (and clone) boards need the old-bootloader CPU variant:

```sh
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old --libraries src/libraries -u -p /dev/ttyUSB0 src/zopapa
```

If uploads fail with repeated `not in sync` errors, that's this — try the `atmega328old` variant. Requires your account to be in the `dialout` group (Linux).

## Command interface

Sent over Serial or Bluetooth (same UART, 9600 baud):

| Command | Function | Example |
|---|---|---|
| `S` | Start | `S` |
| `P` | Stop | `P` |
| `T` | Run autotune | `T` |
| `Vxxx` | Set speed | `V180` |
| `Kxxx` | Set KP | `K22.5` |
| `Dxxx` | Set KD | `D15.3` |
| `R` | Read current parameters | `R` |
| `L` | Load parameters from EEPROM | `L` |
| `W` | Save parameters to EEPROM | `W` |
| `Q` | Live sensor bar test | `Q` |
| `H` | Help | `H` |

See [`docs/FSD.html`](docs/FSD.html) for the full electrical interconnect, BOM, control-loop math, and known quirks.

## Notes

- Firmware comments and Serial output are in Spanish.
- No test suite.
