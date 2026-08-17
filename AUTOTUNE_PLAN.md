# Speed-scheduled autotune — implementation plan

**Status: DRAFT — awaiting bench calibration data (expected 2026-08-18) before
implementation starts.** This is a planning document only; no firmware
changes have been made yet.

## Problem

`autotuneRelay()` (relay-feedback / Åström–Hägglund) identifies KP/KD at a
single operating point — whatever `SPEED` happens to be set to when `T` runs
— and those gains then stay fixed even if `SPEED` is changed afterward (via
`V`). At low `SPEED` this works; at realistic race speed the same gains
under-damp badly enough that the robot's oscillation amplitude exceeds the
sensor bar's physical span (66mm) and it loses the line entirely.

## Why: the plant gain scales with speed

Kinematic model (see chat discussion 2026-08-17 for full derivation), using
the current asymmetric wheel-mixing scheme in `correr()`
(`right = SPEED - corr`, left pinned at `SPEED`, or the mirror image):

```
heading_rate ≈ (Kv / L) · corr        — ~SPEED-independent
y_rate       ≈ v · heading            — v = Kv · SPEED
```

So `corr → lateral position` is a double integrator whose gain is
proportional to `SPEED`. For a PD loop around it:

```
ωn = sqrt(v·k·KP)     →  KP ∝ 1/v   to hold ωn constant
2ζωn = v·k·KD          →  KD ∝ 1/v   to hold ζ constant
```

Both gains should shrink roughly as `1/SPEED` to keep the same closed-loop
damping across speeds — the firmware currently does the opposite (holds them
constant). Turning radius is **not** the constraint: worst-case turn radius
(`corr` saturated, one wheel at 0) works out to `trackwidth/2 ≈ 47.5mm`,
independent of `SPEED`, well under the 200mm tournament minimum. This is a
dynamics/tuning problem, not a geometry problem.

## Known constants (measured 2026-08-17)

| Quantity | Value | Source |
|---|---|---|
| Track width, edge-to-edge | 115mm | measured |
| Wheel width | 20mm | measured |
| Track width, effective (L) | 95mm | edge-to-edge − one wheel width |
| Wheel diameter | 32mm | measured |
| Motor no-load speed | 4000 RPM @ 12V | datasheet |
| Max linear speed at SPEED=255 | ~5 m/s | **rough estimate, not measured** |
| Sensor pitch | 9.5mm | measured (matches 100 units/sensor in `ATRSensors`) |
| Sensor bar span, edge-to-edge | 66mm | measured |
| Sensor bar lookahead (ahead of wheel axle) | 168mm | measured |
| Tightest curve radius, test course | 300mm | measured |
| Tightest curve radius, tournament rules | 200mm | rules |
| Line width | 20mm | measured |
| Wheel encoders / IMU | none | confirmed |

**Pending from tomorrow's bench test:** an actual measured `Kv` (m/s per PWM
unit, ideally sampled at 2-3 `SPEED` values, not just top speed) to replace
the "~5 m/s" guess, and ideally a couple of low-speed relay-autotune runs at
different `SPEED` values to check whether gains really scale as `1/SPEED`
(exponent `n = 1`) or something else — the 168mm lookahead adds phase lead
the simple double-integrator model doesn't capture, so `n` may end up less
aggressive than 1 in practice.

## Design

### New concept: reference gains vs. live gains

- `KP_REF`, `KD_REF`, `SPEED_REF` — the autotune *output*: gains measured by
  the existing relay method, always run at a fixed, safe `SPEED_REF` (a low
  constant, e.g. 100), not whatever `SPEED` the operator last set.
- `KP`, `KD` — unchanged in role: still the live values `correr()` reads
  every cycle. Now *derived* rather than set-once:
  ```
  KP = KP_REF * (SPEED_REF / SPEED) ^ SPEED_SCALING_EXPONENT
  KD = KD_REF * (SPEED_REF / SPEED) ^ SPEED_SCALING_EXPONENT
  ```
- `SPEED_SCALING_EXPONENT` — compile-time const, starts at `1.0` per the
  model above. Deliberately *not* EEPROM-persisted (it's a physical-model
  parameter, not a per-robot tuned value) so it's a one-line edit to refine
  once we have real data, no EEPROM layout change needed for that part.

### Where this hooks in

- **`autotuneRelay()`** (`autotune.ino`): run the relay test at a fixed
  `SPEED_REF` regardless of the live `SPEED` — save/restore `SPEED` around
  the test. Removes "operator ran `T` at the wrong speed" as a hazard, and
  makes `T` safe to invoke at any time regardless of current `SPEED`.
  Output becomes `KP_REF`/`KD_REF` instead of `KP`/`KD` directly, followed
  by an immediate `scheduleGains(SPEED)` call so the live gains update.
- **New `scheduleGains(byte speed)`** (new file `gain_scheduling.ino`,
  matching the existing per-concern file split — `eeprom.ino`,
  `steering.ino`): recomputes `KP`/`KD` from `KP_REF`/`KD_REF`/`SPEED_REF`.
  Called after: autotune completes, `V` command changes `SPEED`, EEPROM
  load (`L` / boot autoload).
- **`V` command (`bt.ino`)**: after setting `SPEED`, call `scheduleGains()`.
- **`K` / `D` commands (`bt.ino`)**: manual override. Setting KP/KD directly
  disables scheduling (new `bool gainSchedulingActive` flag) until the next
  successful autotune or `L`. Simplest option that matches how every other
  manual command in this firmware already behaves as a raw override — no
  back-solving KP_REF from a manually-entered live-speed value.
- **EEPROM (`eeprom.ino`)**: extend the existing PID block to store
  `KP_REF`/`KD_REF`/`SPEED_REF` alongside (or instead of) live `KP`/`KD`.
  Reuses the existing multi-byte magic-marker convention. Layout change —
  old EEPROM contents won't parse as the new format, so this needs a fresh
  `W` save after flashing (same one-time re-save the calibration-flag fix
  already required); will call this out explicitly in the commit.
- **Safety net in the relay test itself**: abort early if the measured
  position error approaches the sensor array's physical edges (near 0 or
  700), independent of the speed-scheduling fix — cheap defense-in-depth
  even though running at a fixed safe `SPEED_REF` should make this rare.

### Explicitly out of scope for this change

- No new sensors (confirmed: no gyro/encoders available).
- No change to the control law itself (still plain PD on `readLine()`
  position) — this is a gain-scheduling fix, not a Stanley/pure-pursuit
  rewrite.
- No automatic speed profiling / cornering slow-down — turning radius
  analysis shows it isn't needed for the stated curve radii.

## Task list

1. [ ] Bench-calibrate `Kv` (measured, not guessed) at 2-3 `SPEED` values.
2. [ ] Run existing relay autotune at a couple of safe low speeds; check
       whether `KP_REF·SPEED` / `KD_REF·SPEED` stays roughly constant
       (confirms or refutes `n = 1`).
3. [ ] Pick `SPEED_REF` and `SPEED_SCALING_EXPONENT` based on the above.
4. [ ] Implement `gain_scheduling.ino` (`scheduleGains()`,
       `gainSchedulingActive` flag).
5. [ ] Modify `autotuneRelay()` to run at fixed `SPEED_REF`, output
       `KP_REF`/`KD_REF`, call `scheduleGains()`.
6. [ ] Wire `scheduleGains()` into `V`, `L`, boot autoload; wire override
       flag into `K`/`D`.
7. [ ] Extend EEPROM layout for `KP_REF`/`KD_REF`/`SPEED_REF`; bump/adjust
       magic marker if needed so old data isn't misread.
8. [ ] Add relay-test-abort safety net for near-edge position error.
9. [ ] Flash, verify EEPROM read-back, verify `R` output matches
       hand-calculated `scheduleGains()` output for a few `SPEED` values.
10. [ ] On-track validation across the speed range; compare oscillation
        amplitude against the fixed-gain baseline.
11. [ ] Update `docs/FSD.html` (§8 EEPROM layout, §7.3 control loop, §8
        autotune section), `docs/MANUAL.html` (AutoTune section), and
        `CLAUDE.md` once behavior is verified on hardware.

## Open questions (resolve before/while implementing)

- Is `SPEED_REF` a fixed compile-time constant, or should it be
  operator-settable? Leaning fixed constant for now — simpler, and autotune
  is already an infrequent, deliberate operation.
- Should the firmware keep an escape hatch to fully disable scheduling
  (e.g. a DIP position) in case it misbehaves on the actual track, given
  there's no automated test suite and this is safety-relevant behavior?
  Leaning "not needed" — `K`/`D` manual override already provides an
  escape hatch — but worth a second look once we see real on-track results.
