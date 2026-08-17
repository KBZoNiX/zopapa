# Speed-scheduled autotune — implementation plan (v2)

**Status: DRAFT, measurement-gated.** Supersedes the v1 plan. No firmware
changes to the control law until Phase 2's decision gate is passed.

**How to use this document:** Phases 0 and 1 are unconditional — implement
them now. Phase 2 is a decision point that requires real data. Phases 3A/3B/3C
are mutually-exclusive-ish branches; **do not implement any of them before
Phase 2 is resolved.** If you are an agent picking this up mid-stream, check
the Phase 1 results table for filled-in numbers before touching Phase 3.

---

## 1. Problem

`autotuneRelay()` (relay feedback, Åström–Hägglund) identifies KP/KD at
whatever `SPEED` is set when `T` runs, and those gains stay fixed when `SPEED`
is later changed via `V`. At low `SPEED` the robot tracks fine. At realistic
race speed the same gains under-damp badly enough that oscillation amplitude
exceeds the sensor bar's 66mm span and the robot leaves the line.

## 2. Competing hypotheses — do not pre-commit

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

## 3. Physical constants

| Quantity | Value | Confidence |
|---|---|---|
| Track width, edge-to-edge | 115mm | measured |
| Wheel width | 20mm | measured |
| Track width, effective (`L`) | 95mm | derived |
| Wheel diameter | 32mm | measured |
| Motor no-load speed | 4000 RPM @ 12V | **datasheet — verify shaft** |
| Gearbox reduction | **UNKNOWN** | **must confirm** |
| Max linear speed @ SPEED=255 | ~5 m/s | **guess — Phase 1 replaces** |
| PWM deadband `S0` | **UNKNOWN** | **Phase 1 measures** |
| Sensor pitch | 9.5mm | measured |
| Sensor bar span | 66mm (±33mm error budget) | measured |
| Sensor lookahead `D` (ahead of axle) | 168mm | measured |
| Loop period `dt` | **UNKNOWN** | **Phase 0 measures** |
| Total loop dead time `T_d` | **UNKNOWN** | Phase 1, optional |
| Tightest curve, test course | 300mm | measured |
| Tightest curve, tournament | 200mm | rules |
| Line width | 20mm | measured |
| Encoders / IMU | none | confirmed |

> **Verify first:** 4000 RPM on a 32mm wheel is 6.7 m/s at the rim, which is
> very fast for a line follower. Confirm whether that figure is motor-shaft or
> output-shaft. If there is a gearbox, every derived speed is wrong by the
> reduction ratio and `Kv` must come from measurement regardless.

## 4. Model — and why the exponent is not a constant

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
  fields over floats to buy depth.
- **T0.2** New serial command (e.g. `Z`) to dump the buffer as CSV over BT.
  Header row with column names. Start/stop control: log continuously into the
  ring, freeze on line-loss or on command.
- **T0.3** Log actual loop period. Record min/max/mean `dt` over a run and
  report it — this is an input to every later calculation and is currently
  unknown.
- **T0.4** `tools/` script to capture the serial dump to `logs/` and plot
  position vs time. Keep it dead simple (pyserial + matplotlib).

**Acceptance:** a full lap can be dumped, plotted, and the oscillation period
read off the plot by eye.

## Phase 1 — Measurement (implement now, unconditional)

- **T1.1 Deadband and `Kv`.** Roll the robot over a measured straight distance
  at `SPEED` ∈ {60, 100, 150, 200, 255} (or the widest safe set), timing each
  run with a stopwatch. Fit `v = Kv·(SPEED − S0)`. Record `S0` and `Kv`.
  Two points are not enough — the deadband intercept needs three or more.
- **T1.2 Multi-speed relay runs.** Run the existing `autotuneRelay()` at 3+
  speeds spanning the widest **safe** range, not two low ones. For each,
  record `SPEED`, `KP_REF`, `KD_REF`, and the measured ultimate period `Tu`.
  Export `Tu` from the relay routine if it is not already reported.
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

## Phase 2 — Decision gate

Pick **one** branch from the Phase 1 data and record the reasoning in the
commit message:

- `n` fits ≈ 1, points collinear, all runs below the zero, failure on
  straights → **Phase 3A** (power-law scheduling, as v1 intended).
- Points not collinear, or `Tu` crosses `2πD/v` inside the operating range →
  **Phase 3B** (closed-form scheduling).
- `n` ≈ 0 and/or failure localises to curve entry → **Phase 3C**. Gain
  scheduling is not the fix; do not implement 3A/3B.
- Oscillation period roughly constant across speeds → H3, dead-time limited.
  No scheduling helps; reduce loop period and filtering, then re-measure.

---

## Phase 3A — Power-law gain scheduling

Reference gains decoupled from live gains:

- `KP_REF`, `KD_REF`, `SPEED_REF` — autotune output, always measured at a
  fixed safe `SPEED_REF`.
- `KP`, `KD` — live values `correr()` reads each cycle, now derived:

```
v_ref  = KV * (SPEED_REF - S0)
v_live = KV * (SPEED    - S0)
KP = KP_REF * pow(v_ref / v_live, n)
KD = KD_REF * pow(v_ref / v_live, n)
```

**Schedule on estimated velocity, not raw `SPEED`.** `SPEED_REF/SPEED` is not
the velocity ratio when a deadband exists; with `S0 ≈ 30`, `100/255 = 0.39`
versus `(100−30)/(255−30) = 0.31` — a 25% error straight into the gain.

### Implementation

- **New `gain_scheduling.ino`** (matches existing per-concern file split):
  `scheduleGains(byte speed)` plus a `bool gainSchedulingActive` flag.
- **`autotuneRelay()` (`autotune.ino`)**: save/restore `SPEED` around the
  test, run at fixed `SPEED_REF`, write `KP_REF`/`KD_REF`, then call
  `scheduleGains(SPEED)`. This makes `T` safe to invoke at any `SPEED` and is
  worth doing **regardless of which Phase 3 branch is chosen.**
- **`V` command (`bt.ino`)**: call `scheduleGains()` after setting `SPEED`.
- **`K`/`D` commands**: raw manual override, sets `gainSchedulingActive =
  false` until the next successful autotune.
- **Relay-test edge abort**: abort if position approaches the array edges
  (near 0 or 700). **Implement this independently of the branch chosen** —
  cheap defense-in-depth.

### Required guards (all missing from v1)

- **`scheduleGains(0)` divides by zero.** Guard the divisor; check whether `V`
  accepts 0 or values below `S0` (where `v_live ≤ 0`). Clamp
  `SPEED ≥ S0 + margin`.
- **Clamp the output.** Low `SPEED` inflates gains without limit — at `n = 1`
  from `SPEED_REF = 100`, `SPEED = 10` gives a ~25× multiplier. Clamp `KP`/`KD`
  to a sane band and report when clamping is active.
- **Confirm `KP`/`KD` are `float`.** If they are integer, a 0.39× factor
  quantizes badly and the whole scheme degrades silently.
- **Make `n` runtime-settable** (new serial command, unpersisted, defaulting
  to the compile-time constant). v1 made it a compile-time const; since the
  explicit intent is to refine it from track data, a reflash per iteration
  will dominate test-session time. `Kv` and `S0` likewise.

## Phase 3B — Closed-form scheduling (if the zero is in range)

Same hooks as 3A; only the gain computation changes. Instead of a power law,
solve for KP/KD giving a target phase margin against

```
P(s) = a · (D·s + v) / s² · e^(-T_d·s)
```

with `a` and `T_d` identified from a straight-line step response (step `corr`
on a straight, fit the position ramp from telemetry — `T_d` is the delay
before the response starts). Handles the regime transition automatically and
costs roughly the same amount of code as 3A. Prefer this if T1.4 shows a
crossing.

## Phase 3C — Curvature handling (if failure is at curve entry)

- Estimate curvature from the low-pass-filtered `corr` signal.
- Reduce `SPEED` proportionally when the estimate exceeds a threshold; restore
  on straights with a short hold to avoid chatter.
- Optionally add a feedforward `corr` term from the curvature estimate.
- This is a control-law change and needs its own validation pass; keep it in a
  separate commit from any scheduling work.

---

## Phase 4 — EEPROM, verification, docs

- **T4.1 EEPROM (`eeprom.ino`)**: store `KP_REF`/`KD_REF`/`SPEED_REF`
  **instead of** live `KP`/`KD`, not alongside — v1 left this ambiguous and
  the two can disagree. Live gains are always derived at load. Reuse the
  multi-byte magic-marker convention; bump the marker so old contents fail to
  parse rather than being misread. Call out the required one-time `W` re-save
  in the commit message.
- **T4.2 Resolve `L` vs manual override.** v1 has `K`/`D` clear
  `gainSchedulingActive`, but `L` re-enables scheduling — so a manually tuned
  set is silently overwritten on load. Decide: either persist the flag, or
  document manual gains as explicitly non-persistent (preferred, simpler).
- **T4.3 Bench verification**: flash, verify EEPROM read-back, verify `R`
  output matches hand-calculated `scheduleGains()` for several `SPEED` values
  including edge cases (`SPEED = 0`, `SPEED = S0`, `SPEED = 255`).
- **T4.4 On-track validation** with telemetry: compare peak error and
  oscillation amplitude against the fixed-gain baseline logs from T1.5, at
  matched speeds.
- **T4.5 Docs**: update `docs/FSD.html` (§7.3 control loop, §8 EEPROM layout
  and autotune), `docs/MANUAL.html` (AutoTune section, new `Z`/`n` commands),
  and `CLAUDE.md` — only after hardware verification.

## Out of scope

- No new sensors (no gyro/encoders available — confirmed).
- No Stanley / pure-pursuit rewrite. The existing PD on `readLine()` position
  stays; the lookahead already provides implicit lead.
- ~~No curve slowdown~~ — **re-opened**, see §5 and Phase 3C.

## Open questions

- `SPEED_REF` fixed constant or operator-settable? Leaning fixed; autotune is
  an infrequent deliberate operation.
- Escape hatch to fully disable scheduling on the track? `K`/`D` override
  probably suffices, but re-check after the first on-track session.
- Does battery sag between the start and end of a session move `Kv` enough to
  matter? Telemetry from T4.4 across a charge cycle will show it.

## Suggested commit sequence

1. Telemetry ring buffer + `Z` dump + loop-period reporting (Phase 0)
2. Relay edge-abort safety net (branch-independent)
3. Autotune runs at fixed `SPEED_REF`, save/restore `SPEED` (branch-independent)
4. `Kv`/`S0` measurement support if any firmware help is needed (Phase 1)
5. — **decision gate** —
6. Chosen Phase 3 branch
7. EEPROM layout change + `L`/override resolution
8. Docs
