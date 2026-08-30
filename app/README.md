# Zopapa Control

Android companion app for the Zopapa line-following robot. Talks to the
robot over classic Bluetooth SPP (the same command protocol documented in
`../src/zopapa/bt.ino` and `../docs/MANUAL*.html`) so the team captain can
load gain presets, run the trackside preflight check, and make quick
parameter tweaks from a phone — matching what `docs/Reglamento-Carreras.md`
§7.4 actually allows during a match.

Two screens:
- **Home** — Start/Stop, gain presets (load/save), manual KP/KD/Speed,
  Preflight, Calibrate, AutoTune, and an auto-populating run summary after
  each stop. This is the only screen meant for use during a match.
- **Advanced/Practice** — the rest of the protocol (sensor bar test,
  open-loop drive, telemetry dump, decimation/relay-amplitude/autotune-
  timeout tuning, the separate manual EEPROM slot, and a raw command
  terminal). Bench/practice only — see the in-app warning banner.

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
`flutter build apk --debug`, and `flutter build apk --release` all pass as
of this writing. The parser tests actually caught three real bugs before
this was ever run against hardware — worth knowing about if you touch
`lib/protocol/parsers.dart`:
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

This is a strong argument for not trusting hand-written protocol parsers
without tests, even ones that look obviously correct on read-through.

## Known gaps / things to verify on real hardware

None of the above has touched an actual robot yet — only unit tests against
hand-transcribed sample strings and a build/compile check. Specifically:

- The exact multi-line shape of a few responses (autotune failure paths,
  whether "AutoTune Finalizado." always prints) was inferred from the
  firmware research rather than captured from a live device — if `T`
  responses don't parse cleanly, check `parseAutotuneBlock` in
  `lib/protocol/parsers.dart` against what the robot actually sends.
- No battery-voltage/status query exists in the firmware, so the app can't
  show battery level.
- `Q` (sensor bar test) blocks the whole firmware, not just the run state —
  the app sends it via `sendRaw` (bypasses the command queue) precisely so
  it doesn't hang everything else waiting for a reply that won't come until
  someone presses the physical pushbutton.
