# Speed-scheduled autotune — implementation plan (v2.1)

**Status: DRAFT, measurement-gated.** Verification pass over
`PLAN-speed-scheduling-v2.md` — same scope and decision structure, unchanged
except where noted below. No firmware changes to the control law until
Phase 2's decision gate is passed. v2.md is left untouched; this file is a
new document.

## Changelog vs v2 (this pass)

Source-code and PCB-layout verification only — no new bench measurements
were taken. Scope was: cross-check every physical constant in §3 against
what the repo can actually confirm, and stress-test Phase 0/Phase 1
viability against real hardware limits (SRAM, existing timeout constants).

- **§3 Physical constants** — annotated each row with what source
  inspection can and cannot confirm. Sensor pitch/bar span verified
  precisely from the PCB layout (were rounded in v2). Loop period's
  "UNKNOWN" was too strong — the *nominal* value is a named constant in
  source; only the *achieved* value is genuinely unmeasured. Everything
  chassis/motor-related (track width, wheel diameter, motor RPM, gearbox,
  lookahead `D`) is **not derivable from this repo** — no CAD/BOM for the
  chassis or motors exists in-tree, so those stay exactly as measured/
  guessed in v2, just labeled "unverifiable from source" instead of left
  ambiguous.
- **Phase 0 — new §0.1 SRAM budget.** The plan's "size it to what SRAM
  allows (target ≥3-5s of run at loop rate)" doesn't add up on this MCU:
  compiled today, the sketch already uses 344 of 2048 bytes of SRAM. A
  naive per-cycle record (timestamp + position + corr + KP + KD as
  specified in T0.1) is ~15 bytes; at the nominal 2ms loop, 3-5s needs
  1500-2500 samples — 22-37KB. That's off by more than an order of
  magnitude from what's physically on the chip. This is a real blocker for
  T0.1 as literally written and needed a concrete fix (below), not just a
  "prefer int16_t" hand-wave.
- **Phase 1 — new §1.1 viability notes.** Found a live bug that directly
  threatens T1.2's data: `AUTOTUNE_TIMEOUT_MS = 2000` but its own inline
  comment says `// seguridad: 25 s timeout` — a 12.5x mismatch, almost
  certainly a units slip (missing a zero) rather than intentional. At 2
  real seconds, any relay run whose ultimate period `Pu` exceeds roughly
  440ms will time out before collecting the 9 peaks the routine requires,
  and fail with "not enough peaks" — indistinguishable from a real physics
  result unless this is checked first. Also flagged: `autotuneRelay()` has
  no position-based abort today, and Phase 1 explicitly pushes it to
  speeds it's never been run at ("widest safe range") — the "edge abort"
  safety net that v2 assigns to the Phase 3A section but calls
  "branch-independent" and schedules before the decision gate should
  really be a Phase 1 prerequisite, not something read only once you reach
  the Phase 3A section. Also corrected the abort's target range (posición
  is centered ±350, not raw 0..700) and flagged an ambiguity in T1.1's
  open-loop-vs-line-following measurement method.
- Everything else (§1, §2, §4, Phase 2 decision gate, Phase 3A/B/C, Phase
  4, Out of scope, Open questions, commit sequence) is carried over from
  v2 **unchanged** — those aren't physical-constant or Phase 0/1 viability
  material and weren't in scope for this pass.

---

## 1. Problem

*(unchanged from v2)*

`autotuneRelay()` (relay feedback, Åström–Hägglund) identifies KP/KD at
whatever `SPEED` is set when `T` runs, and those gains stay fixed when `SPEED`
is later changed via `V`. At low `SPEED` the robot tracks fine. At realistic
race speed the same gains under-damp badly enough that oscillation amplitude
exceeds the sensor bar's 66mm span and the robot leaves the line.

## 2. Competing hypotheses — do not pre-commit

*(unchanged from v2)*

The v1 plan committed to hypothesis H1 before data could distinguish it from
H2/H3. All three predict "worse at high speed"; they require different fixes.

| # | Hypothesis | Prediction | Fix |
|---|---|---|---|
| H1 | Plant gain scales with `v`, so fixed gains over-drive at speed | Relay-measured `KP_REF·v` ≈ const across speeds | Gain scheduling (3A/3B) |
| H2 | Curve-entry transient: step curvature disturbance, peak error ∝ `v` | Error blows up **at curve entry**, not on straights | Curvature feedforward / curve slowdown (3C) |
| H3 | Dead-time limited: fixed loop delay `T_d`, disturbance frequency ∝ `v` | Oscillation period ≈ const across speeds, ≈ `2·T_d`-ish | Reduce `T_d`, increase lookahead, cap speed |

Phase 1 telemetry distinguishes these directly: log position error against
time and check **where** on the course the excursion happens and **how the
oscillation period moves with speed**.

## 3. Physical constants — verified against source where possible

| Quantity | Value | Confidence | Verification (this pass) |
|---|---|---|---|
| Track width, edge-to-edge | 115mm | measured | **Not derivable from repo** — no chassis CAD/BOM in-tree. Trust the bench measurement. |
| Wheel width | 20mm | measured | Same — not in repo. |
| Track width, effective (`L`) | 95mm | derived | Arithmetic only (115 − 20); depends on the two rows above. |
| Wheel diameter | 32mm | measured | Same — not in repo. |
| Motor no-load speed | 4000 RPM @ 12V | **datasheet — verify shaft** | Not in repo — motors are off-board, connected via J8/J9 on the main PCB with no MPN/datasheet reference in the schematic. Self-consistency check only: 4000 RPM on a 32mm wheel = 6.7 m/s rim speed, and the plan's own §3 note already derives this — internally consistent, doesn't confirm the figure is correct. |
| Gearbox reduction | **UNKNOWN** | **must confirm** | Confirmed genuinely unknown — no gearbox spec anywhere in the schematic, BOM, or docs. Correctly flagged in v2; nothing to add. |
| Max linear speed @ SPEED=255 | ~5 m/s | **guess — Phase 1 replaces** | Unverifiable without `Kv`/gearbox data. Leave as-is. |
| PWM deadband `S0` | **UNKNOWN** | **Phase 1 measures** | Confirmed by reading `DRV8833::speed()` (SLOW_DECAY/locked-antiphase): net motor drive is linear in the `speed` argument (duty on one pin fixed at `HIGH`, the other at `255-|speed|`), so `S0` is genuinely a static-friction/deadband effect, not a PWM-encoding artifact — T1.1's linear fit `v = Kv·(SPEED−S0)` is the right model. |
| Sensor pitch | 9.5mm | measured | **Verified from PCB layout** (`hardware/line_sensor_pcb/ATR-8A.kicad_pcb`): sensors U1-U8 (OnSemi CASE100CY) sit at x = 93.345, 102.87, 112.395, ... 160.02mm, i.e. exactly 9.525mm apart (= 3/8", a standard part spacing). Round to 9.5mm is fine for the model; use 9.525mm if being precise. |
| Sensor bar span | 66mm (±33mm error budget) | measured | **Refined from PCB layout**: 7 gaps × 9.525mm = 66.675mm edge-to-edge (sensor 0 to sensor 7), giving a ±33.3mm error budget — matches v2's number to within rounding. Also cross-checked against `steering.ino`/`ATRSensors.cpp`: `centro_de_linea = 350` (for 8 sensors), and 350 units × (9.525mm / 100 units-per-sensor) = 33.3mm — consistent with the position units the control loop actually runs in. |
| Sensor lookahead `D` (ahead of axle) | 168mm | measured | **Not derivable from repo** — this is a distance between two separate physical boards on the chassis; no chassis dimension data exists in-tree. Trust the bench measurement. |
| Loop period `dt` | **UNKNOWN** | **Phase 0 measures** | **Partially wrong as stated.** The *nominal* target is not unknown — `LOOP_MS = 2` is a named constant in `zopapa.ino` (also documented in `docs/FSD.html` §7.3: "Runs every LOOP_MS = 2 ms"). What's genuinely unmeasured is the *achieved* period: `loop()` gates on `millis() - lastLoopTime > LOOP_MS`, so real dt is quantized by `millis()`'s 1ms resolution and stretched by per-cycle overhead (8-sensor `readLine()` pass, ~250µs per the library's own header comment, plus button/DIP polling). Phase 0's T0.3 is still the right task — just measuring the gap between a *known* 2ms target and the *actual* period, not measuring from zero. |
| Total loop dead time `T_d` | **UNKNOWN** | Phase 1, optional | Unverifiable from source; needs the step-response test described in Phase 3B. No change. |
| Tightest curve, test course | 300mm | measured | Not in repo (track geometry, not chassis/firmware). |
| Tightest curve, tournament | 200mm | rules | External, not in repo. |
| Line width | 20mm | measured | Not in repo. |
| Encoders / IMU | none | confirmed | **Confirmed from source**: `zopapa.ino`'s only position sensing is the `ATRSensors` reflectance array; no encoder/IMU library, pins, or ISR exist anywhere in `src/`. |

> **Verify first** *(unchanged from v2)*: 4000 RPM on a 32mm wheel is 6.7
> m/s at the rim, which is very fast for a line follower. Confirm whether
> that figure is motor-shaft or output-shaft. If there is a gearbox, every
> derived speed is wrong by the reduction ratio and `Kv` must come from
> measurement regardless.

## 4. Model — and why the exponent is not a constant

*(unchanged from v2 — model derivation doesn't depend on anything this pass
could verify further; `D = 168mm` and `L = 95mm` remain trusted
measurements, not repo-derivable.)*

The controller does not observe lateral offset `y`; it observes the sensor bar
`D = 168mm` ahead of the axle, so `e = y + D·θ`. With `a = Kv/L`:

```
e(s)/corr(s) = a · (D·s + v) / s²
```

There is a **zero at `ω_z = v/D`**, and the plant changes character across it:

- **Below `ω_z`** (`ω ≪ v/D`): behaves as `a·v/s²` — double integrator, gain
  ∝ `v`. Holding damping constant needs `KP, KD ∝ 1/v`, i.e. exponent `n = 1`.
- **Above `ω_z`** (`ω ≫ v/D`): collapses to `a·D/s` — single integrator, gain
  **independent of `v`**. Exponent `n = 0`; no scheduling needed.

Because `ω_z` rises with `v`, low-speed runs sit *above* the zero and
high-speed runs sit *below* it. **Fitting one exponent to two low-speed points
and extrapolating to race speed samples the wrong regime and will
under-estimate `n`.** With `D = 168mm` at 1.8× the effective track width, the
lead is large and the zero is plausibly inside the loop bandwidth.

**Diagnostic (free, from Phase 1 data):** compare relay-measured ultimate
period `Tu` at each speed against `2πD/v`:

- `Tu < 2πD/v` → oscillating above the zero → lookahead-dominated → `n ≈ 0` →
  **gain scheduling is not the fix**; go to H2/H3.
- `Tu > 2πD/v` → below the zero → `n ≈ 1` → scheduling applies.
- Crossing over between speeds → the regime transition is inside your operating
  range → **use 3B (closed form), not 3A (power law)**.

Also note: `Kv` sags with battery voltage, so any schedule keyed to `SPEED`
drifts over a session. Do not chase small exponent differences.

## 5. Correction to the v1 out-of-scope argument

*(unchanged from v2)*

v1 dismissed curve slowdown because worst-case steady-state turn radius
(`corr` saturated, one wheel stopped) is `trackwidth/2 ≈ 47.5mm`, far inside
the 200mm minimum. **That argument does not rule out H2.** It describes
steady-state agility; the failure mode is a *transient*. Entering a 200mm
curve at speed is a step curvature disturbance whose peak error grows with `v`
against a fixed ±33mm budget. Curve slowdown is **back in scope pending Phase
1 data**.

---

## Phase 0 — Telemetry (implement now, unconditional)

Nothing downstream is measurable without this. v1's task 10 asked for a
comparison against baseline with no instrument to make it.

- **T0.1** Add a fixed-size RAM ring buffer, logged once per control cycle:
  `(millis/micros, position from readLine(), corr, KP, KD, SPEED)`. Size it to
  what SRAM allows (target ≥ 3–5 s of run at loop rate); prefer `int16_t`
  fields over floats to buy depth. **See §0.1 below — as specified this does
  not fit; use the revised record layout.**
- **T0.2** New serial command (e.g. `Z`) to dump the buffer as CSV over BT.
  Header row with column names. Start/stop control: log continuously into the
  ring, freeze on line-loss or on command.
- **T0.3** Log actual loop period. Record min/max/mean `dt` over a run and
  report it — this measures the gap between the known 2ms nominal `LOOP_MS`
  and what's actually achieved; still a required task, see §3 above.
- **T0.4** `tools/` script to capture the serial dump to `logs/` and plot
  position vs time. Keep it dead simple (pyserial + matplotlib).

**Acceptance:** a full lap can be dumped, plotted, and the oscillation period
read off the plot by eye.

### §0.1 SRAM budget (new — this pass)

Compiled as-is (`arduino-cli compile --fqbn arduino:avr:nano`), the current
sketch reports:

```
Global variables: 344 bytes (16%) — 1704 bytes free for locals, of 2048 total
```

That 1704 bytes is shared between the heap (the `String btCommand` buffer,
the two 8-byte `malloc`'d calibration arrays) and the call stack — a large
new global array eats directly into the same pool the rest of the firmware
needs at runtime. Leaving a conservative ~400 bytes of headroom, a new ring
buffer has roughly **1300 bytes** to work with, not "however much fits" —
and the record layout in T0.1 doesn't fit inside that:

| Record as specified in T0.1 | Bytes |
|---|---|
| timestamp (millis/micros) | 4 (won't fit in int16 as an absolute value — see below) |
| position | 2 |
| corr | 2 |
| KP | 4 (float) |
| KD | 4 (float) |
| SPEED | 1 |
| **Total** | **~17 bytes/record** |

At the nominal 2ms loop, "≥3-5s" needs 1500-2500 records — **25-42KB**,
roughly 20x the entire chip's SRAM, let alone the 1300 bytes actually
available. This isn't a rounding-error gap; T0.1 as literally written cannot
be built on this MCU.

**Revised record layout:**

- Drop `KP`/`KD`/`SPEED` from the per-cycle record. Within one telemetry
  capture they only change on an explicit command (`K`/`D`/`V`) or an
  autotune run — log them once via `Serial.print` when the `Z` dump starts
  (or whenever they change, if that's cheap), not every 2ms.
- Store `dt` (delta from the previous sample, not an absolute timestamp) as
  `int16_t`. At a ~2ms loop this comfortably fits (would need >32s between
  samples to overflow); an absolute `millis()` needs a 32-bit field for no
  benefit here.
- That leaves a 6-byte record: `dt (int16) + position (int16) + corr
  (int16)`. 1300 / 6 ≈ **216 samples ≈ 430ms of data at full 2ms
  resolution** — short of "3-5s" by roughly an order of magnitude if every
  cycle is logged.
- To reach multi-second coverage, **decimate**: log every Nth control cycle
  instead of every cycle. Oscillation periods worth seeing "by eye" on a
  plot are expected in the tens-to-hundreds-of-ms range (subject to actual
  Phase 1 `Tu` data), so sampling every 10th cycle (~20ms/sample) still
  resolves periods well above ~40ms by Nyquist, and stretches the same 216
  samples to ~4.3s — inside the target range. Make the decimation factor a
  runtime-settable value (same "don't hardcode what you'll want to retune
  at track-side" principle the plan already applies elsewhere, e.g. Phase
  3A's guidance on `n`), not a compile-time constant, so the
  duration/resolution trade-off can be adjusted between runs without a
  reflash.
- This also validates the plan's "buffer then dump" architecture (rather
  than streaming live over serial): at 9600 baud (≈960 bytes/s), even the
  full-resolution 6-byte-record case would need ~2000+ bytes/s doing live
  CSV writes at 500Hz — not achievable without dropping the control loop.
  Buffering in RAM and dumping in one shot after `estado` is stopped (as
  T0.2 already specifies) is the correct call, not just a simplification.

---

## Phase 1 — Measurement (implement now, unconditional)

- **T1.1 Deadband and `Kv`.** Roll the robot over a measured straight distance
  at `SPEED` ∈ {60, 100, 150, 200, 255} (or the widest safe set), timing each
  run with a stopwatch. Fit `v = Kv·(SPEED − S0)`. Record `S0` and `Kv`.
  Two points are not enough — the deadband intercept needs three or more.
  **See §1.1 below for a method ambiguity worth resolving before running this.**
- **T1.2 Multi-speed relay runs.** Run the existing `autotuneRelay()` at 3+
  speeds spanning the widest **safe** range, not two low ones. For each,
  record `SPEED`, `KP_REF`, `KD_REF`, and the measured ultimate period `Tu`.
  Export `Tu` from the relay routine if it is not already reported.
  **See §1.1 below — check `AUTOTUNE_TIMEOUT_MS` first, this can silently
  invalidate low-speed runs.**
- **T1.3 Exponent fit.** Plot `log(KP_REF)` vs `log(v)` using `v` from T1.1
  (not raw `SPEED`). Read `n` off the slope. **If the points are not
  collinear, that is itself the result** — record the curvature, do not force
  a line through it.
- **T1.4 Zero-crossing check.** For each speed, compute `2πD/v` and compare
  against `Tu` (§4). Record which side of the zero each run sits on.
- **T1.5 Failure localisation.** With telemetry running, do baseline
  fixed-gain runs at increasing speed until the line is lost. From the plot:
  does error grow on straights (H1/H3) or spike at curve entry (H2)? Does the
  oscillation period stay roughly constant across speeds (H3) or scale (H1)?

**Results table — fill in before proceeding:**

| SPEED | v (m/s) | KP_REF | KD_REF | Tu (s) | 2πD/v (s) | above/below zero |
|---|---|---|---|---|---|---|
| | | | | | | |

`S0 = ______`  `Kv = ______ (m/s per PWM count)`  `n (fitted) = ______`
`loop dt = ______ ms`  `failure location = straight / curve-entry / both`

### §1.1 Phase 1 viability notes (new — this pass)

**`AUTOTUNE_TIMEOUT_MS` mismatch — check/fix before T1.2.** In
`autotune.ino`:

```
const unsigned long AUTOTUNE_TIMEOUT_MS = 2000;   // seguridad: 25 s timeout
```

The constant is 2000ms; the comment claims 25s. A 12.5x gap like this reads
as a missing zero, not an intentional design choice. `autotuneRelay()`
requires `RELAY_SETTLE_CYCLES + RELAY_MEASURE_CYCLES = 9` peaks before it
reports success; at the actual 2-second cap, that means the average
half-period between peaks must stay under ~220ms, or the run times out and
reports "Autotune failed: not enough peaks detected." T1.2 explicitly asks
for runs across "the widest safe range" including speeds not tried before —
if any of those produce a genuinely slower ultimate period `Pu` (plausible
at low `SPEED`, and exactly the regime the model in §4 says sits closer to
the double-integrator/undamped extreme), the run fails for a timing reason
having nothing to do with the physics being measured, and that failure is
indistinguishable from a real result without checking this constant first.
Recommend resolving whether 2000 or 25000 (or something else) was intended
before trusting any T1.2 "insufficient peaks" result as data.

**Edge-abort has no home before Phase 1 runs, but Phase 1 needs it.** v2
describes a position-based abort for `autotuneRelay()` ("abort early if the
measured position approaches the sensor array's physical edges") under the
Phase 3A section, while also calling it "branch-independent" and placing it
before the decision gate in the suggested commit sequence. As written today,
`autotuneRelay()` has **no** such guard — a saturated/off-line reading
during the relay test is not detected or handled specially. T1.2 is the
first task that runs the relay method at speeds beyond what's already been
validated ("widest safe range" — by definition includes new territory), so
this is a Phase 1 prerequisite in practice, not just Phase 3A prep work.
Recommend implementing it alongside T1.2, before running it, rather than
waiting for the Phase 3A section to be reached.

One correction to the abort's target range: `posicion` in both `correr()`
and `autotuneRelay()` is *centered* — `centro_de_linea (350) −
readLine()` — so it ranges roughly **−350…+350**, not the raw `readLine()`
range of 0..700 that the Phase 3A text names. The abort should trigger near
`|posicion| ≈ 350`, not near 0 or 700.

**T1.1 method ambiguity.** "Roll the robot over a measured straight
distance at SPEED" is workable with existing commands (`S`/`V`) only if the
test line segment is straight enough that closed-loop steering correction
stays ~0 throughout the run — otherwise the measured distance/time reflects
a path with lateral wiggle, not the open-loop wheel-speed-vs-PWM mapping the
`v = Kv·(SPEED−S0)` model actually wants. There's no existing firmware
command to drive both motors open-loop at a fixed `SPEED` outside of
line-following mode (`S` starts full `correr()`, not a raw drive). This
doesn't block T1.1 — a sufficiently straight, well-centered test segment
makes closed-loop correction negligible — but it's worth deciding explicitly
which method is being used before recording `S0`/`Kv`, since v2's own
Phase 3A commit sequence already anticipates this ("`Kv`/`S0` measurement
support if any firmware help is needed") without resolving it.

---

*Phases 2, 3A, 3B, 3C, 4, "Out of scope", "Open questions", and "Suggested
commit sequence" are unchanged from v2 and are not reproduced here — see
`PLAN-speed-scheduling-v2.md`. This pass did not touch them; they don't
depend on physical-constant verification or Phase 0/1 viability.*
