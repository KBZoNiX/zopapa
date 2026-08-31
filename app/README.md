# Zopapa Control

Android companion app for the Zopapa line-following robot. Talks to the
robot over classic Bluetooth SPP (the same command protocol documented in
`../src/zopapa/bt.ino` and `../docs/MANUAL*.html`) so the team captain can
load gain presets, run the trackside preflight check, and make quick
parameter tweaks from a phone — matching what `docs/Reglamento-Carreras.md`
§7.4 actually allows during a match.

Two screens:
- **Home** — gain presets (load/save), manual KP/KD/Speed, Start/Stop,
  Preflight, Calibrate, AutoTune, and an auto-populating run summary after
  each stop. This is the only screen meant for use during a match.
- **Advanced/Practice** — the rest of the protocol (sensor bar test,
  open-loop drive, telemetry dump, decimation/relay-amplitude/autotune-
  timeout tuning, the separate manual EEPROM slot, and a raw command
  terminal). Bench/practice only — see the in-app warning banner.

Dark theme (black ground, red for primary actions/Start-Stop, blue for
Preflight/Calibrar/AutoTune, yellow for the active-preset highlight) and app
icon match the robot artwork in `assets/`; see `lib/main.dart`'s
`_zopapaTheme()` if you need to touch colors.

## Prerequisites

Built and verified against: Flutter 3.47.2 (stable), Gradle 9.3.1, AGP
9.1.0, Kotlin 2.4.0, JDK 17 — these are whatever the installed Flutter
version's own `android.tmpl` template pins (`android/*.gradle.kts`,
`android/gradle/wrapper/gradle-wrapper.properties`), not hand-picked
versions, so they'll drift forward automatically the next time someone
regenerates the scaffold against a newer Flutter. `compileSdk`/`minSdk`/
`targetSdk` all come from `flutter.*Version` (currently resolves to API 36,
NDK r28c — Flutter's Gradle plugin downloads those itself via `sdkmanager`
on first build if missing), not a hardcoded number in this repo.

- Flutter SDK (stable channel).
- Android SDK command-line tools + JDK 17. A device or emulator running
  API 23+ (physical device strongly recommended — classic Bluetooth SPP
  doesn't work on most emulators).
- The robot's Bluetooth module already paired with the phone once, from
  Android's system Bluetooth settings (the app only lists bonded devices —
  it doesn't do discovery/pairing itself, see the plan notes below).

## First-time setup

```
cd app
flutter pub get
flutter build apk --debug --target-platform android-arm64   # or `flutter run`
```

`android/local.properties` (gitignored) needs to point at your SDKs, e.g.:
```
sdk.dir=/path/to/Android/sdk
flutter.sdk=/path/to/flutter
```
`flutter pub get` (or `flutter create .`) generates this file for you the
first time you point Flutter at an Android SDK; `android/gradlew` +
`gradle/wrapper/gradle-wrapper.jar` are gitignored too and get regenerated
the same way (or via `gradle wrapper --gradle-version <version from
gradle-wrapper.properties>` if you need them without a full Flutter build).

**Memory note**: on a machine with ~16GB RAM or less, the stock
`org.gradle.jvmargs=-Xmx8G` in `android/gradle.properties` (inherited
from Flutter's template) plus a separate 8G-heap Kotlin compile daemon can
swap-thrash badly and make a from-scratch build take 30+ minutes instead of
~2-3. This repo's `android/gradle.properties` already caps both to 2G/1G
and disables the Gradle daemon — if you copy settings from a fresh
`flutter create` template, keep that override or re-add it. Building a
single ABI (`--target-platform android-arm64`) instead of the default fat
multi-ABI APK also helps a lot on constrained machines.

## Architecture notes

- `lib/transport/` — the `RobotTransport` interface plus the Android classic-
  Bluetooth implementation (`AndroidBluetoothTransport`, backed by a small
  Kotlin platform channel in `android/app/.../BtLink.kt` — a custom channel
  rather than a third-party plugin, to avoid depending on an
  often-unmaintained package for a competition-day tool). This is the seam
  for a possible future Linux/BlueZ transport; nothing above it should need
  to change to add one.
- `lib/protocol/` — `RobotClient` (one queued async method per firmware
  command, matching/parsing the exact response text) and `parsers.dart`
  (pure functions, unit-tested in `test/parsers_test.dart`).
- `lib/state/robot_state.dart` — app-wide `ChangeNotifier`. Also where the
  "is the robot currently running" client-side tracking lives — the
  firmware has no query command for that; see the doc comment there.
- `lib/screens/` — Connect, Home, Advanced, plus shared widgets.

## Build status

`flutter analyze`, `flutter test` (16 tests, all in `test/parsers_test.dart`),
`flutter build apk --debug`, and `flutter build apk --release` all pass as of
this writing. The app has since been run against the real robot (a
Bluetooth-paired HC-05-style module, phone-side testing on a Samsung Galaxy
S22) — connect, Preflight, Calibrate, preset load/save, manual gains, and
Start/Stop (motors confirmed spinning, both closed-loop `S` and open-loop
`O`) all verified working end-to-end. AutoTune and the Advanced screen's
bench commands (`Q`/`Z`/`N`/`A`/`M`/`L`/`W`, raw terminal) have not yet been
exercised live — see "Known gaps" below.

That first hardware pass caught real bugs no amount of unit testing would
have — worth knowing about if you touch `android/app/.../BtLink.kt` or
`lib/protocol/`:
- **Crash on every connect attempt**: `BtLink.connect()` called
  `BluetoothAdapter.cancelDiscovery()` before opening the socket (a common
  "just in case" precaution), which throws `SecurityException` unless the
  app holds `BLUETOOTH_SCAN` — a permission this app deliberately never
  requests, since it only connects to already-bonded devices and has no
  discovery UI. Fixed by removing the call.
- **Every command after the first connect hung forever, with no error and
  no timeout**: `connect()` and `write()` shared one single-thread
  executor, but `connect()`'s task ended by running the *blocking* read
  loop inline, on that same thread — which never returns for the life of
  the connection. That permanently starved the executor, so `write()`
  could never be scheduled again. Confirmed by adding temporary
  instrumentation showing the write task's start timestamp stuck at zero
  while the read loop sat blocked on its first `read()`. Fixed by moving
  the read loop to its own dedicated thread.
- **Calibrate always ran out its full timeout, right around when the
  physical 3-second sweep actually finished**: `BtLink`'s read loop
  decoded incoming bytes one-at-a-time via `Byte.toChar()`, which mangles
  multi-byte UTF-8 sequences. The firmware's Spanish strings are UTF-8, and
  `'Calibración finalizada.'` contains `ó` (2 bytes) — so the exact-string
  match the app waits for never succeeded. Fixed by buffering raw bytes
  per line and decoding as UTF-8 only once a full line is available.
- **Manual KP/KD/Speed fields kept showing stale typed values after
  loading a preset**: the fields relied on `TextField`'s `hintText` to
  show live values, but Material only reveals `hintText` once the field's
  label floats (on focus or non-empty text) — so an untouched field just
  showed its label, and a previously-edited one kept showing whatever was
  typed, forever. Fixed by writing the live value into the controller's
  text directly whenever `lastGains` changes from something other than
  that field's own edit.

Also from the earlier, hardware-independent unit-test pass — three parser
bugs in `lib/protocol/parsers.dart`, worth knowing about if you touch it:
- `parsePreflightBlock`/`parseRunSummaryBlock` checked "activo"/"activa"
  via `.contains(...)`, which false-positives on "des**activo**"/
  "des**activa**da" — fixed with an exact-match-after-the-colon check
  (`_endsWithState`).
- `parseRunSummaryBlock`'s `intAfter` helper double-counted the `=` in
  keys like `'muestras='`, so the regex required `muestras==123` and
  always fell back to 0.
- The saturation-cycles line contains its own stray `=` inside
  `(>=340)`, which a naive first-`=`-in-the-line regex matched before the
  real `= <n>` assignment — fixed by anchoring on the `)` that precedes it.

This is a strong argument for not trusting hand-written protocol/transport
code without tests and a real device, even code that looks obviously
correct on read-through.

## Known gaps / things to verify on real hardware

- **AutoTune** (`T`) has not been run against the live robot — the exact
  multi-line response shape (failure paths, whether "AutoTune Finalizado."
  always prints) was inferred from firmware research rather than captured
  from a device. If `T` responses don't parse cleanly, check
  `parseAutotuneBlock` in `lib/protocol/parsers.dart` against what the
  robot actually sends.
- The Advanced/Practice screen's bench commands (`Q`, `Z`, `N`, `A`, `M`,
  `L`, `W`, and the raw terminal) haven't been exercised live either —
  lower priority since none of them are legal mid-match per
  `docs/Reglamento-Carreras.md` §7.4, but worth a pass before relying on
  them for practice sessions.
- No battery-voltage/status query exists in the firmware, so the app can't
  show battery level.
- `Q` (sensor bar test) blocks the whole firmware, not just the run state —
  the app sends it via `sendRaw` (bypasses the command queue) precisely so
  it doesn't hang everything else waiting for a reply that won't come until
  someone presses the physical pushbutton.
- Release builds are currently signed with the debug keystore
  (`android/app/build.gradle.kts`'s `signingConfig` for the `release`
  build type is still the Flutter template's placeholder) — fine for
  sideloading onto your own phone, not set up for wider distribution.
