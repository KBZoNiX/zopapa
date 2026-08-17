# Speed-scheduled autotune — implementation plan (v3.1)

**Status: DRAFT, measurement-gated.** Supersedes `PLAN-speed-scheduling-v2.md`,
`PLAN-speed-scheduling-v2_1.md` and `-v3.md`. No firmware changes to the control
law until the Phase 2 decision gate is passed.

**How to use this document.** Phase −1 (source checks) and Phases 0–1 are
unconditional — implement them now. Phase 2 is a decision point requiring real
data. Phases 3A/3B/3C are branches; **do not implement any of them before
Phase 2 is resolved.** If you are an agent picking this up mid-stream, check
the Phase 1 results table for filled-in numbers before touching Phase 3.

> ### ⚠ Read this before anything else
>
> `docs/FSD.html` §11.1 contains an "Empirical tuning reference" table giving
> KP/KD at six speeds from 150 to 255. **Those gains rise with speed** — KP
> roughly as `v^+1.7`. The v1 and v2 plans assume gains should *fall* as `1/v`.
>
> §11.1 is now explicitly flagged in the FSD as old, unverified data — earlier
> test sessions, not re-confirmed against current calibration, chassis or
> firmware. So it does **not** refute the model on its own. What it does do is
> remove any justification for treating `n = +1` as the default and building on
> it. Sign disagreement between a stale-but-structured dataset and an
> unvalidated model means **neither is trusted until Phase 1 measures the
> exponent directly.** See §2.1.

## Changelog

**v3.1 (this revision)** — reweights §11.1 following the FSD provenance caveat
("old data, unverified… treat as a historical starting point, not a validated
reference, until re-measured"):

- **§2.1 rewritten.** The table drops from "contradicts H1, must be reconciled"
  to "a prior worth testing". Adds the analysis of *which* confounds it is and
  isn't vulnerable to — most named confounds are multiplicative and move the
  intercept, not the log-log slope — plus two internal tells suggesting parts of
  the table were derived rather than measured.
- **§2.2 weakened.** Constant `Td` is still a reasonable design choice, but it
  can no longer be described as something the author empirically converged on;
  the upper three rows have `KD = 5·KP` exactly, which reads as a rule of thumb.
- **T1.6 reframed** as the task that converts §11.1 from stale notes into
  current data, and promoted in priority accordingly.
- **Phase 2 gate**: the "sign conflict → stop" branch is softened to "sign
  conflict → T1.6 arbitrates", since a stale table can no longer block on its
  own.

**v3** — merged the v2.1 verification pass and added six findings from
`docs/FSD.html`:

- **New §2.1**: the §11.1 empirical table as prior data. Now an explicit input
  to the Phase 2 gate.
- **New §2.2**: `KD/KP ≈ 4.3–5.0` across that table — near-constant derivative
  time. Possibly a better thing to schedule than two independent gains.
- **T1.2 rewritten**: `RELAY_AMPLITUDE = 0.5×SPEED` means excitation scales
  with operating point, so oscillation amplitude scales as `SPEED²` and runs
  across the sweep are not comparable. Use a fixed absolute `d`.
- **New Phase −1**: three source-level checks that can invalidate Phase 1 data,
  including whether `MAX_CORRECTION`/`RELAY_AMPLITUDE` actually track `V`.
- **§0.1 refined**: Sonnet's SRAM budget and decimation scheme kept; the
  Nyquist justification for the decimation factor is corrected — peak
  *amplitude* needs far more than 2 samples/period.
- **§1.1 extended**: the `AUTOTUNE_TIMEOUT_MS` bug doesn't just cause failures,
  it *biases* the §4 zero-crossing diagnostic toward one answer.
- **New §4.1**: `KD` is not `dt`-normalised, so loop jitter modulates the
  effective derivative gain and `Td` must be converted before any
  continuous-time calculation.

**v2.1 (Sonnet, source/PCB verification pass)** — SRAM budget blocker for T0.1;
`AUTOTUNE_TIMEOUT_MS` comment/code mismatch; edge-abort promoted to a Phase 1
prerequisite; `posicion` range corrected to ±350; sensor pitch/span verified
from the KiCad layout; T1.1 open-loop-vs-closed-loop method ambiguity. All
carried into v3.

---

## 1. Problem

`autotuneRelay()` (relay feedback, Åström–Hägglund) identifies KP/KD at
whatever `SPEED` is set when `T` runs, and those gains stay fixed when `SPEED`
is later changed via `V`. At low `SPEED` the robot tracks fine. At realistic
race speed the same gains under-damp badly enough that oscillation amplitude
exceeds the sensor bar's ~66mm span and the robot leaves the line.

## 2. Competing hypotheses — do not pre-commit

The v1 plan committed to H1 before data could distinguish it from H2/H3. All
predict "worse at high speed"; they require different fixes.

| # | Hypothesis | Prediction | Fix |
|---|---|---|---|
| H1 | Plant gain scales with `v`, fixed gains over-drive at speed | `KP_REF·v` ≈ const across speeds; failure on straights | Gain scheduling (3A/3B), `n ≈ +1` |
| H2 | Curve-entry transient: step curvature disturbance, peak error ∝ `v` | Error spikes **at curve entry**, not on straights | Curvature feedforward / curve slowdown (3C) |
| H3 | Dead-time limited: fixed loop delay `T_d`, disturbance frequency ∝ `v` | Oscillation period ≈ const across speeds | Reduce `T_d`, increase lookahead, cap speed |

### 2.1 Prior data: the §11.1 empirical table — stale, but pointing the other way

`docs/FSD.html` §11.1, "Empirical tuning reference":

| SPEED | 150 | 160 | 180 | 200 | 220 | 255 |
|---|---|---|---|---|---|---|
| KP | 1.15 | 1.33 | 1.75 | 2.0 | 2.4 | 2.8 |
| KD | 5 | 5.5 | 7.5 | 10 | 12 | 14 |

Log-log slopes over the full range:

```
KP:  ln(2.8/1.15) / ln(255/150) = 0.890 / 0.531 = +1.68
KD:  ln(14/5)     / ln(255/150) = 1.030 / 0.531 = +1.94
```

In this plan's convention `KP = KP_REF·(v_ref/v_live)^n`, gains rising with `v`
means **`n ≈ −1.7`**. The §4 model predicts `n = +1`.

**Provenance (per the FSD):** old data, unverified — earlier test sessions, not
re-confirmed against the current sensor calibration, chassis, or firmware
revision. A historical starting point, not a validated reference.

That caveat matters, but it does not make the table uninformative, because the
named confounds do not all attack the same thing:

**What the confounds move — intercept vs slope.** Calibration drift, a scale
change in `readLine()`, a different line contrast, and most chassis changes act
as a *multiplicative* factor on position or on plant gain. On a log-log plot
that shifts the whole line up or down; it does not tilt it. In particular the
FSD §7.2 calibration bug — `calibrate()` only ever widened min/max, so every
"fresh" sweep was a union with the previous one, verified as max climbing
~220 → ~250 — is exactly this kind of confound: it rescales position, so it
changes the level of KP that works, not the direction in which KP has to move
with speed. **The intercept of §11.1 is untrustworthy. Its slope is more robust
than the caveat suggests.**

**What could genuinely flip the slope**, and is worth checking:

- A change in lookahead `D` (sensor bar moved, different chassis). `D` sets the
  zero at `ω_z = v/D`, which is the one thing in §4 that makes the exponent
  speed-dependent. A large change here is the most plausible way old gains could
  scale differently from current ones.
- A change to the mixing or clamp behaviour in `correr()` between firmware
  revisions — see T-1.1, which asks whether `MAX_CORRECTION` tracks `V` at all.
- The author tuning to a different objective. Rising gains are what you'd expect
  from someone tuning for enough bandwidth to make the corner, rather than for
  constant damping — which is H2/H3, not H1.

**Two internal tells, which weaken parts of the table independently of age:**

- The upper three rows have `KD = 5.00·KP` *exactly* (2.0/10, 2.4/12, 2.8/14),
  while the lower three sit at 4.14–4.35. That reads as a rule of thumb applied
  to the upper cluster rather than six independent tunings — so **KD's +1.9
  slope is not independent evidence**, it is KP's slope times a fixed ratio.
  Weight KP's trend, not KD's.
- The KP slope is not constant: ≈ +2.3 across 150–180 and ≈ +1.4 across
  200–255. Either two sessions stitched together, or a real regime change of the
  kind §4 predicts. Don't quote "+1.7" as if it were a clean power law.

**Net weight.** §11.1 is not evidence strong enough to refute the model, and no
longer justifies a "stop" at the gate on its own. But the model is not validated
either — it is a derivation, not a measurement, and §4 already admits its
exponent is regime-dependent. **A stale dataset and an unvalidated model
disagreeing in sign means the exponent is simply unknown, and must be measured
before anything is built on it.** That is what Phase 1 is for, and it is why
T1.6 exists.

Note also: the proposed `SPEED_REF = 100` sits below every row in the table, and
the firmware default `KP/KD = 2/10` matches the **SPEED 200** row rather than
anything near 100. Worth a moment's thought when choosing `SPEED_REF`, though
with §11.1 downgraded this is a curiosity rather than a constraint.

### 2.2 `KD/KP` is roughly constant — a reasonable choice, not a finding

| SPEED | 150 | 160 | 180 | 200 | 220 | 255 |
|---|---|---|---|---|---|---|
| KD/KP | 4.35 | 4.14 | 4.29 | 5.00 | 5.00 | 5.00 |

Derivative time is roughly flat (in loop-cycle units — see §4.1) while both
gains move by 2.4×. Given the exact-5.00 tell above, treat this as **a design
choice worth adopting on its own merits, not as an empirical result**: holding
`Td = KD/KP` fixed and scheduling a single loop gain halves the number of
quantities to fit and is far more robust to noisy relay data than scheduling KP
and KD independently. Fold it into whichever Phase 3 branch is chosen, and let
T1.2 confirm whether measured `KD_REF/KP_REF` actually stays flat across the
sweep.

---

## 3. Physical constants

Confidence column reflects what source/PCB inspection can actually confirm.
Nothing chassis- or motor-related is derivable from the repo — no CAD or BOM
for either exists in-tree — so those stay as bench measurements.

| Quantity | Value | Confidence / verification |
|---|---|---|
| Track width, edge-to-edge | 115mm | Measured. Not in repo. |
| Wheel width | 20mm | Measured. Not in repo. |
| Track width, effective (`L`) | 95mm | Derived (115 − 20). |
| Wheel diameter | 32mm | Measured. Not in repo. |
| Motor no-load speed | 4000 RPM @ 12V | **Datasheet — verify shaft.** Motors are off-board via J8/J9, no MPN in schematic. |
| Gearbox reduction | **UNKNOWN** | **Must confirm.** Nothing in schematic, BOM or docs. |
| Max linear speed @ SPEED=255 | ~5 m/s | **Guess — Phase 1 replaces.** |
| PWM deadband `S0` | **UNKNOWN** | **Phase 1 measures.** `DRV8833::speed()` is linear in its argument (SLOW_DECAY / locked-antiphase), so `S0` is genuinely a static-friction effect, not a PWM-encoding artifact — `v = Kv·(SPEED−S0)` is the right model. |
| Sensor pitch | 9.525mm | **Verified from PCB** (`hardware/line_sensor_pcb/ATR-8A.kicad_pcb`): U1–U8 at x = 93.345 … 160.02mm, exactly 9.525mm apart (3/8"). |
| Sensor bar span | 66.675mm (±33.3mm budget) | **Verified**: 7 gaps × 9.525mm. Cross-checks against firmware: `centro_de_linea = 350` × (9.525mm / 100 units-per-sensor) = 33.3mm. |
| Sensor lookahead `D` (ahead of axle) | 168mm | Measured. Board-to-board chassis distance, not in repo. |
| Loop period `dt` | nominal 2ms, achieved **UNKNOWN** | `LOOP_MS = 2` in `zopapa.ino` (FSD §7.3). `loop()` gates on `millis()`, so achieved period is quantised to 1ms and stretched by per-cycle overhead (8-channel `readLine()` ≈ 250µs, button/DIP polling). **Phase 0 measures the achieved value and its jitter.** |
| Total loop dead time `T_d` | **UNKNOWN** | Needs the step-response test in Phase 3B. |
| Position range | −350 … +350 | **Verified**: `centro_de_linea (350) − readLine()`, FSD §7.3. *Not* 0..700 — v2 had this wrong. |
| Tightest curve, test course | 300mm | Measured. |
| Tightest curve, tournament | 200mm | Rules. |
| Line width | 20mm | Measured. |
| Encoders / IMU | none | **Confirmed from source**: only `ATRSensors` provides position; no encoder/IMU library, pins or ISR anywhere in `src/`. |

> **Verify first:** 4000 RPM on a 32mm wheel is 6.7 m/s rim speed — very fast
> for a line follower. Confirm motor-shaft vs output-shaft. If there's a
> gearbox, every derived speed is wrong by the reduction ratio, and `Kv` must
> come from measurement regardless.

## 4. Model — and why the exponent is not a constant

The controller does not observe lateral offset `y`; it observes the sensor bar
`D = 168mm` ahead of the axle, so `e = y + D·θ`. With `a = Kv/L`:

```
e(s)/corr(s) = a · (D·s + v) / s²
```

There is a **zero at `ω_z = v/D`**, and the plant changes character across it:

- **Below `ω_z`**: behaves as `a·v/s²` — double integrator, gain ∝ `v`. Constant
  damping needs `KP, KD ∝ 1/v`, i.e. `n = +1`.
- **Above `ω_z`**: collapses to `a·D/s` — single integrator, gain **independent
  of `v`**. `n = 0`; no scheduling needed.

Because `ω_z` rises with `v`, low-speed runs sit *above* the zero and high-speed
runs *below* it. **Fitting one exponent to two low-speed points and
extrapolating to race speed samples the wrong regime.** With `D = 168mm` at 1.8×
the effective track width, the lead is large and the zero is plausibly inside
the loop bandwidth.

**Diagnostic (free, from Phase 1 data):** compare relay-measured ultimate period
`Pu` at each speed against `2πD/v`:

- `Pu < 2πD/v` → above the zero → lookahead-dominated → `n ≈ 0` → **scheduling
  is not the fix**; go to H2/H3.
- `Pu > 2πD/v` → below the zero → `n ≈ +1` → scheduling applies.
- Crossing over inside the operating range → **use 3B (closed form), not 3A**.

⚠ **This diagnostic is currently rigged** — see §1.1. Fix
`AUTOTUNE_TIMEOUT_MS` before trusting any `Pu` value.

Note also that `Kv` sags with battery voltage, so any schedule keyed to `SPEED`
drifts over a session. Don't chase small exponent differences.

### 4.1 `KD` is not `dt`-normalised

`correction = KP·position + KD·(position − last_position)` — a raw per-cycle
delta, no division by `dt` (FSD §7.3). Two consequences:

- Continuous-time derivative time is `Td = (KD/KP)·dt`, so §2.2's ratio of
  ~4.3–5.0 corresponds to `Td ≈ 8.6–10ms` at the nominal 2ms loop. Convert
  before using it in any phase-margin calculation (Phase 3B).
- Loop jitter directly modulates effective derivative gain. `millis()`-gated
  timing quantised to 1ms on a 2ms target is potentially ±50%. This makes
  T0.3's jitter measurement load-bearing, not nice-to-have — and if jitter is
  large, normalising `KD` by measured `dt` may be worth doing on its own merits,
  independently of any scheduling work.

## 5. Correction to the v1 out-of-scope argument

v1 dismissed curve slowdown because worst-case steady-state turn radius (`corr`
saturated, one wheel stopped) is `trackwidth/2 ≈ 47.5mm`, far inside the 200mm
minimum. **That does not rule out H2.** It describes steady-state agility; the
failure mode is a *transient*. Entering a 200mm curve at speed is a step
curvature disturbance whose peak error grows with `v` against a fixed ±33mm
budget. Curve slowdown is **back in scope**, and §2.1 raises its prior
probability considerably.

---

## Phase −1 — Source checks (do first; cheap, and can invalidate Phase 1)

- **T-1.1 Do `MAX_CORRECTION` and `RELAY_AMPLITUDE` actually track `V`?**
  FSD §11 lists them as `2×SPEED` and `0.5×SPEED`. If they are `const`
  initialised from the default `SPEED = 100`, then `V180` changes base duty but
  leaves the clamp at 200 and relay amplitude at 50. That would be a live bug,
  and it would also mean steering authority already varies with speed in a way
  §4 doesn't model. One grep.
- **T-1.2 Fix `AUTOTUNE_TIMEOUT_MS`.** `= 2000` with an adjacent comment reading
  "25 s" (FSD §8, §13). Resolve which was intended before any T1.2 run. See
  §1.1 for why this matters more than it looks.
- **T-1.3 Confirm `KP`/`KD` are `float`.** EEPROM layout stores them as 4-byte
  floats (FSD §10), so they almost certainly are — but a 0.3× scale factor on
  an integer gain degrades silently, so verify rather than assume.

## Phase 0 — Telemetry (unconditional)

Nothing downstream is measurable without this. v1's task 10 asked for a
comparison against baseline with no instrument to make it.

- **T0.1** Fixed-size RAM ring buffer logged per control cycle. **See §0.1 for
  the record layout — the naive layout does not fit on this MCU.**
- **T0.2** New serial command (e.g. `Z`) dumping the buffer as CSV over the
  UART, with a header row. Log continuously into the ring; freeze on line-loss
  or on command. The existing `MAX_CICLOS_SIN_DETECTAR` line-loss cutoff
  (FSD §7.4) is the natural freeze trigger — reuse it rather than writing a new
  detector.
- **T0.3** Measure and report achieved loop period: min/max/mean `dt` over a
  run. This is the gap between the known 2ms `LOOP_MS` target and reality, and
  it feeds §4.1.
- **T0.4** `tools/` script to capture the dump into `logs/` and plot position vs
  time (pyserial + matplotlib; keep it dumb).

**Acceptance:** a full run can be dumped, plotted, and the oscillation period
read off the plot by eye.

### §0.1 SRAM budget and record layout

Compiled as-is (`arduino-cli compile --fqbn arduino:avr:nano`):

```
Global variables: 344 bytes (16%) — 1704 bytes free for locals, of 2048 total
```

That 1704 bytes is shared between heap (the `String btCommand` buffer, the two
8-byte `malloc`'d calibration arrays) and stack. Leaving ~400 bytes headroom,
the ring buffer has roughly **1300 bytes**. The record as v2 specified it
(timestamp 4 + position 2 + corr 2 + KP 4 + KD 4 + SPEED 1 ≈ 17 bytes) at 2ms
for 3–5s needs 25–42KB — more than 20× the entire chip. **Not a rounding gap;
v2's T0.1 could not be built.**

Revised layout:

- **Drop `KP`/`KD`/`SPEED` from the per-cycle record.** Within one capture they
  change only on explicit command (`K`/`D`/`V`) or an autotune run. Print them
  once in the `Z` dump header.
- **Store `dt` as a delta, `int16_t`**, not an absolute `millis()`. At a ~2ms
  loop this needs >32s between samples to overflow.
- Result: **6-byte record** (`dt`, `position`, `corr`, all `int16_t`).
  1300 / 6 ≈ **216 samples ≈ 430ms** at full 2ms resolution.
- **Decimate** to reach multi-second coverage: log every Nth cycle. Make N
  runtime-settable, not a compile-time constant — same principle as `n` in
  Phase 3A.

**Choosing N — Nyquist is not the constraint.** v2.1 justified N = 10 (~20ms
sampling) as resolving periods above ~40ms by Nyquist. That is the aliasing
limit and it is far too loose here: Nyquist recovers *frequency*, not *peak
amplitude*. Sampling a sinusoid at 10 points per cycle under-reads its peak by
up to ~5%, and at 5 points per cycle by ~19% — and peak amplitude is exactly
what T1.2 (`a` for `Ku = 4d/πa`) and T4.4 (amplitude vs baseline) depend on.
Under-read `a` inflates `Ku`, which is the dangerous direction.

Split the use cases:

| Purpose | N | Coverage | Rationale |
|---|---|---|---|
| T1.2 relay runs, T4.4 amplitude | 1–2 | 0.4–0.9s | Amplitude fidelity; a relay burst is short anyway |
| T1.5 failure localisation, full laps | 8–16 | 3.5–7s | Only need *where* on the course, not exact peaks |

**Buffer-then-dump is the right architecture**, not just a simplification: at
9600 baud (≈960 B/s), live CSV at 500Hz would need >2000 B/s and would break the
control loop. If dumps become painful, consider a higher baud rate for
USB-tethered sessions — but check the Bluetooth module's fixed rate first, since
the UART is shared (FSD §9).

## Phase 1 — Measurement (unconditional)

- **T1.0 Implement the relay edge-abort *before* running T1.2.** Abort the relay
  test when `|posicion|` approaches 350. `autotuneRelay()` has no such guard
  today, and T1.2 explicitly pushes it to speeds never tried before. This is a
  Phase 1 prerequisite, not Phase 3A prep. See also T1.2's amplitude note below
  — without this abort you cannot tell a valid run from a saturated one.
- **T1.1 Deadband and `Kv`.** Time the robot over a measured straight at `SPEED`
  ∈ {60, 100, 150, 200, 255} (widest safe set). Fit `v = Kv·(SPEED − S0)`.
  Three or more points minimum — two cannot resolve the intercept.
  *Method ambiguity to resolve first:* there is no firmware command to drive
  both motors open-loop at a fixed `SPEED` (`S` starts full `correr()`,
  FSD §9). A sufficiently straight, well-centred segment makes closed-loop
  correction negligible, but decide explicitly which method you're using before
  recording numbers, and consider adding a raw-drive command if not.
- **T1.2 Multi-speed relay runs**, 3+ speeds spanning the widest safe range.
  Record `SPEED`, `KP_REF`, `KD_REF`, and measured `Pu`. Export `Pu` from the
  relay routine if not already reported.

  ⚠ **Use a fixed absolute relay amplitude `d` for this sweep, not
  `0.5×SPEED`.** With the default, excitation scales with operating point. The
  relay maths still works (`Ku = 4d/πa` is a ratio), but the *excursion* does
  not stay bounded: for a plant whose gain scales with `v`, driven at a
  limit-cycle frequency set by phase rather than gain, `a ∝ d·g ∝ SPEED²`.
  Doubling SPEED quadruples the excursion against a fixed ±350 budget.
  Saturation then clips `a`, which inflates `Ku`, which returns gains that are
  too aggressive **precisely at the speeds where that is most dangerous, and
  silently**. A fixed `d` decouples excitation from operating point and makes
  runs comparable.
- **T1.3 Exponent fit.** Plot `log(KP_REF)` vs `log(v)` using `v` from T1.1, not
  raw `SPEED`. Read `n` off the slope. **Non-collinearity is itself the
  result** — record the curvature, don't force a line. Then compare against
  §2.1. If the sign disagrees with §11.1, that is not automatically a blocker
  (the table is stale) — but it does mean T1.6 has to arbitrate before any
  branch is implemented.
- **T1.4 Zero-crossing check.** For each speed compute `2πD/v` and compare
  against `Pu` (§4). Record which side of the zero each run sits on. Requires
  T-1.2 done first.
- **T1.5 Failure localisation.** With telemetry running (N = 8–16), do baseline
  fixed-gain runs at increasing speed until the line is lost. From the plot:
  does error grow on straights (H1/H3) or spike at curve entry (H2)? Does
  oscillation period stay roughly constant across speeds (H3) or scale (H1)?
- **T1.6 Re-measure §11.1 — the arbitrating test.** Now that the FSD flags the
  table as unverified, this is the task that converts it from stale notes into
  current data, and it is the tie-breaker whenever T1.3's fitted `n` disagrees
  in sign with it. Hand-set KP/KD from the table at three speeds spanning
  150–255 and run each with telemetry, alongside autotune-derived gains at the
  same speeds. Record peak error and oscillation amplitude for both.
  - §11.1 gains **outperform** autotune's, and do so more clearly as speed
    rises → the rising-gain trend is real under current hardware. `n` is
    negative; H1 as written is wrong. Strong pull toward 3C.
  - §11.1 gains **underperform** or are indistinguishable → the table was
    tuned against a configuration that no longer exists. Discard it and let
    T1.3/T1.4 decide alone.
  - Both sets are bad at high speed → neither is a tuning problem. Look at
    H2/H3 and at T1.5's failure localisation.

  Cheap, needs no new firmware beyond `K`/`D`/`V` and the Phase 0 telemetry, and
  worth running before the more delicate T1.2 sweep.

**Results table — fill in before proceeding:**

| SPEED | v (m/s) | KP_REF | KD_REF | KD/KP | Pu (s) | 2πD/v (s) | above/below zero | saturated? |
|---|---|---|---|---|---|---|---|---|
| | | | | | | | | |

```
S0 = ______        Kv = ______ m/s per PWM count
n (fitted) = ______        n (from §11.1, stale, KP only) ≈ −1.7 (−2.3 low / −1.4 high)
achieved dt = ______ ms (mean) / ______ ms (jitter)
failure location = straight / curve-entry / both
§11.1 gains vs autotune gains at matched speed: ______
```

### §1.1 `AUTOTUNE_TIMEOUT_MS` biases the diagnostic, not just the yield

In `autotune.ino`:

```c
const unsigned long AUTOTUNE_TIMEOUT_MS = 2000;   // seguridad: 25 s timeout
```

Constant says 2000ms; comment says 25s. A 12.5× gap reads as a missing zero.
`autotuneRelay()` needs `RELAY_SETTLE + RELAY_MEASURE = 3 + 6 = 9` peaks
(FSD §8, §11); peaks are half-periods apart, so the run needs roughly `4–4.5 ×
Pu` seconds. **At a real 2s cap, observable `Pu` is capped around 0.44–0.5s.**

The failure mode is worse than "some runs fail":

At `SPEED = 100`, on the current `~5 m/s @ 255` guess, `v ≈ 2 m/s` and
`2πD/v = 2π(0.168)/2 ≈ 0.53s`. That is *just above* the timeout's observable
ceiling. So the timeout systematically censors every run where `Pu > 2πD/v` —
which is precisely the outcome that would indicate "below the zero, `n ≈ +1`".
**Every surviving run will look like `n ≈ 0` whether or not it is.** T1.4 is
meaningless until T-1.2 is done.

---

## Phase 2 — Decision gate

Pick **one** branch and record the reasoning in the commit message. §11.1 (§2.1)
informs this gate but no longer blocks it on its own; **T1.6 is what resolves a
sign conflict.**

- **Sign conflict with §11.1, unresolved by T1.6** — fitted `n` positive while
  the table says negative, and T1.6 is ambiguous → **do not implement any
  branch yet.** Re-check T-1.1 (does the correction clamp track `V`?), T1.2
  saturation, and whether lookahead `D` differs from the §11.1 era. A model
  wrong about *direction* will not be rescued by tuning its exponent.
- **Sign conflict resolved by T1.6 in favour of §11.1** — the old gains
  demonstrably work better, and better still at higher speed → `n` is negative;
  §4's derivation is missing something. Do **not** implement 3A with `n = +1`.
  Go to 3C, and treat the rising-gain trend as evidence for a
  disturbance-rejection framing rather than a damping one.
- `n` fits ≈ +1, collinear, all runs below the zero, failure on straights, and
  T1.6 shows §11.1 doesn't replicate → **Phase 3A**, power-law scheduling.
- Non-collinear, or `Pu` crosses `2πD/v` inside the operating range →
  **Phase 3B**, closed form.
- `n ≈ 0` and/or failure localises to curve entry → **Phase 3C**. Scheduling is
  not the fix; do not implement 3A/3B.
- Oscillation period roughly constant across speeds → H3, dead-time limited. No
  scheduling helps; reduce loop period and filtering (and consider §4.1's `dt`
  normalisation), then re-measure.

Whichever branch is chosen, apply §2.2: **schedule one loop gain and hold
`Td = KD/KP` constant**, rather than scheduling KP and KD separately.

---

## Phase 3A — Power-law scheduling

- `KP_REF`, `KD_REF`, `SPEED_REF` — autotune output, always measured at a fixed
  safe `SPEED_REF`.
- `KP`, `KD` — live values `correr()` reads each cycle, derived:

```
v_ref  = KV * (SPEED_REF - S0)
v_live = KV * (SPEED     - S0)
KP = KP_REF * pow(v_ref / v_live, n)
KD = KP * TD_RATIO                  // per §2.2, rather than scaling KD alone
```

**Schedule on estimated velocity, not raw `SPEED`.** With a deadband,
`SPEED_REF/SPEED` is not the velocity ratio: at `S0 ≈ 30`, `100/255 = 0.39`
versus `(100−30)/(255−30) = 0.31` — a 25% error straight into the gain.

### Implementation

- **New `gain_scheduling.ino`** (matches the existing per-concern file split):
  `scheduleGains(byte speed)` plus a `bool gainSchedulingActive` flag.
- **`autotuneRelay()`**: save/restore `SPEED` around the test, run at fixed
  `SPEED_REF`, write `KP_REF`/`KD_REF`, then call `scheduleGains(SPEED)`. Makes
  `T` safe to invoke at any `SPEED`. **Worth doing regardless of branch.**
- **`V` (`bt.ino`)**: call `scheduleGains()` after setting `SPEED`.
- **`K`/`D`**: raw manual override; sets `gainSchedulingActive = false` until the
  next successful autotune.

### Required guards

- **`scheduleGains(0)` divides by zero.** Guard the divisor; check whether `V`
  accepts 0 or values below `S0` (where `v_live ≤ 0`). Clamp
  `SPEED ≥ S0 + margin`.
- **Clamp the output.** Low `SPEED` inflates gains without limit — at `n = 1`
  from `SPEED_REF = 100`, `SPEED = 10` gives ~25×. Clamp and report when
  clamping is active.
- **`n`, `Kv`, `S0`, `TD_RATIO` runtime-settable** (new serial command,
  unpersisted, defaulting to compile-time constants). These are explicitly
  intended to be refined from track data; a reflash per iteration would dominate
  session time.

## Phase 3B — Closed-form scheduling (if the zero is in range)

Same hooks as 3A; only the gain computation changes. Solve for KP/KD giving a
target phase margin against

```
P(s) = a · (D·s + v) / s² · e^(-T_d·s)
```

with `a` and `T_d` identified from a straight-line step response (step `corr` on
a straight, fit the position ramp from telemetry; `T_d` is the delay before the
response starts). Handles the regime transition automatically, costs about the
same code as 3A. Remember §4.1 when converting `KD` to continuous-time `Td`.

## Phase 3C — Curvature handling (if failure is at curve entry)

- Estimate curvature from the low-pass-filtered `corr` signal.
- Reduce `SPEED` when the estimate exceeds a threshold; restore on straights
  with a short hold to avoid chatter.
- Optionally add a feedforward `corr` term from the curvature estimate.
- Control-law change — needs its own validation pass, separate commit from any
  scheduling work.

---

## Phase 4 — EEPROM, verification, docs

- **T4.1 EEPROM (`eeprom.ino`)**: store `KP_REF`/`KD_REF`/`SPEED_REF` **instead
  of** live `KP`/`KD`, not alongside — the two can disagree. Live gains derived
  at load. The current layout (FSD §10) has KP at 4, KD at 8, SPEED at 12, with
  gaps at 3 and 13–15 before calibration data at 16 — room exists, but bump the
  magic marker so old contents fail to parse rather than being misread. Note the
  required one-time `W` re-save in the commit message.
- **T4.2 Resolve `L` vs manual override.** `K`/`D` clear `gainSchedulingActive`,
  but `L` re-enables it — a manually tuned set gets silently overwritten on load.
  Either persist the flag, or document manual gains as explicitly non-persistent
  (preferred, simpler).
- **T4.3 Bench verification**: flash, verify EEPROM read-back, verify `R` matches
  hand-calculated `scheduleGains()` for several `SPEED` values including edges
  (`SPEED = 0`, `SPEED = S0`, `SPEED = 255`).
- **T4.4 On-track validation** with telemetry at N = 1–2: compare peak error and
  oscillation amplitude against the T1.5 baseline logs at matched speeds, and
  against the §11.1 hand-set gains from T1.6.
- **T4.4b Refresh or retire §11.1.** T1.6 produces exactly the measurement the
  FSD's caveat asks for. Either replace the table with re-measured values and
  drop the "unverified" note, or delete it and say why — leaving a stale table
  in the docs after measuring the real thing is how it gets re-trusted by the
  next reader.
- **T4.5 Docs**: update `docs/FSD.html` (§7.3 control loop, §8 autotune, §10
  EEPROM layout, §11 constants, §13 open items — the timeout mismatch can be
  struck once fixed), `docs/MANUAL.html` (AutoTune section, new `Z` and
  parameter commands), and `CLAUDE.md`. Only after hardware verification.

## Out of scope

- No new sensors (no gyro/encoders — confirmed from source).
- No Stanley / pure-pursuit rewrite. PD on `readLine()` position stays; the
  168mm lookahead already provides implicit lead.
- ~~No curve slowdown~~ — **re-opened**, see §5 and §2.1.

## Open questions

- `SPEED_REF` fixed constant or operator-settable? Leaning fixed. §2.1 notes
  100 sits below every row of §11.1, but with that table downgraded this is no
  longer a reason to move it.
- Has the sensor bar lookahead `D` changed since the §11.1 sessions? It's the
  one parameter that could genuinely tilt the gain-vs-speed slope rather than
  just shift its level (§2.1). If it has, that alone may explain the
  disagreement — and it also means the current 168mm is the only value the §4
  zero-crossing analysis may use.
- Escape hatch to fully disable scheduling on the track? `K`/`D` override
  probably suffices; re-check after the first on-track session.
- Does battery sag across a session move `Kv` enough to matter? T4.4 telemetry
  across a charge cycle will show it.
- Should `KD` be normalised by measured `dt` (§4.1) regardless of branch? Depends
  on the jitter number from T0.3.

## Suggested commit sequence

1. Source checks and the `AUTOTUNE_TIMEOUT_MS` fix (Phase −1)
2. Telemetry ring buffer + `Z` dump + loop-period/jitter reporting (Phase 0)
3. Relay edge-abort at `|posicion| → 350` (T1.0, branch-independent)
4. *(no firmware)* Run T1.6 — re-measure §11.1 against autotune gains
5. Fixed-amplitude relay option for the T1.2 sweep
6. Autotune runs at fixed `SPEED_REF`, save/restore `SPEED` (branch-independent)
7. Raw open-loop drive command, if T1.1 needs it
8. — **decision gate** —
9. Chosen Phase 3 branch, with `Td` held constant per §2.2
10. EEPROM layout change + `L`/override resolution
11. Docs, including refreshing or retiring §11.1 (T4.4b)
