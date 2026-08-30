# Tournament stopgap — pre-competition hardening

**Context.** Phase −1/0 code from `PLAN-speed-scheduling-v3_1.md` is implemented,
but no Phase 1 measurements have been taken. Tournament is in one week. This
document covers what to change in the meantime and — more importantly — what
**not** to change.

**Governing principle.** You cannot choose `n` without data, so do not ship
anything that depends on `n`. Everything below is either a diagnostic, a
fail-safe, or an operator-speed improvement. None of it alters the nominal
control law. A second goal: make the tournament itself produce usable Phase 1
data, so the week isn't a write-off for the main plan.

---

## Tier 0 — Do this first, one line

**`gainSchedulingActive` must default to `false` in the tournament build.**

If the Phase 3A scaffolding is in the tree with a placeholder exponent, the
robot will silently rescale gains the moment anyone touches `V`. A guessed `n`
with the wrong sign (see §2.1 of the main plan — the direction is genuinely
unknown) is worse than no scheduling at all. Compile it out or force the flag
off at boot, and verify with `R` after a `V` command that KP/KD did not move.

Keep the two branch-independent changes that are strictly safer than the old
behaviour:

- autotune running at a fixed `SPEED_REF` with save/restore of `SPEED`
- the relay edge-abort at `|posicion| → 350`

---

## Tier 1 — Zero control-law risk, high tournament value

### T-S1. Run-summary statistics (replaces telemetry for now)

The full ring buffer + CSV dump + plotting harness is what you can't run. You
don't need it to get a comparable number out of each run. Keep four or five
accumulators updated in the control loop and print them on stop:

- `peak |posicion|` — the single most useful number you can have trackside
- `mean |posicion|` (running sum + sample count)
- number of cycles at saturation (`|posicion| ≥ 340`)
- number of line-loss events, and total cycles lost
- `min` / `max` / `mean` achieved loop `dt`

Cost is a few tens of bytes of RAM and no dump protocol — print over BT to a
phone when the robot stops. This gives you a scalar to compare tuning attempts
against instead of eyeballing, which is the entire point of Phase 0, at maybe 5%
of the work.

**It also generates real Phase 1 data.** Peak error and saturation count across
a range of speeds, on a real course, is most of what T1.5 (failure
localisation) needs. Log the numbers by hand after every run this week and at
the tournament — that's a dataset you'd otherwise spend a session collecting.

### T-S2. Calibration quality check

After `C` completes, report per-sensor `min`, `max`, and `max−min` spread, and
warn if any channel's spread falls below a threshold. Venue lighting will not
match your workshop, and a single dirty or failing sensor changes the effective
position scale without any obvious symptom until the robot is on the track.

Pure diagnostics, no control impact, and it's the failure mode most likely to
cost you a run.

### T-S3. Preflight command

One command (e.g. `P`) that prints everything you'd want to confirm on the
starting line:

- EEPROM parsed OK / fell back to defaults
- calibration present, and the T-S2 spread summary
- active KP, KD, SPEED, and whether scheduling is off
- achieved loop `dt` from a short idle sample
- battery voltage, **if** you have a free analog input — with all eight QTR
  channels on A0–A7 you may not, so treat this as optional

Reduces the odds of racing a config you didn't intend.

### T-S4. EEPROM fail-safe

If the magic marker doesn't parse — which it won't, the first time you flash the
new layout — the firmware must fall back to known-good compiled-in defaults and
**say so loudly**, not run with zeros or garbage. Verify this path deliberately
by flashing to a board with stale EEPROM before you rely on it.

### T-S5. Gain presets

At a tournament you tune between runs under time pressure, and retyping `K`/`D`/
`V` is slow and error-prone. Store 3–4 `(SPEED, KP, KD)` triples in EEPROM and
select with a single command or a DIP position:

- a conservative qualifying profile
- a mid profile
- an aggressive final profile

This is storage plus selection — no new control maths, no dependency on `n`. It
is also the piece of infrastructure that makes the tournament data from T-S1
comparable, because you'll know exactly which triple produced which numbers.

### T-S6. Seed the presets from §11.1

You can't measure the exponent this week, so use the best available prior. The
FSD flags §11.1 as unverified, and it is — but "historical starting point" is
exactly the sanctioned use here. Load a few rows as presets and hand-tune from
there rather than from nothing:

| Profile | SPEED | KP | KD |
|---|---|---|---|
| Conservative | 150 | 1.15 | 5 |
| Mid | 200 | 2.0 | 10 |
| Aggressive | 255 | 2.8 | 14 |

Treat these as starting points to adjust by feel, not as targets. Note that the
firmware default `KP/KD = 2/10` already matches the SPEED 200 row, which is
mildly reassuring about the table's provenance.

**And this doubles as T1.6.** Running these against autotune-derived gains at
matched speeds, with T-S1's peak-error numbers, is the arbitrating test from the
main plan — done at the tournament rather than in a dedicated session. If the
§11.1 ladder holds up better at high speed than autotune's output does, you'll
have resolved the sign question before Phase 1 even starts.

---

## Tier 2 — Only if you have track time to eyeball the result

These do change behaviour. Each is validatable by watching the robot for a few
minutes; none needs instrumentation. Skip any you can't actually test.

### T-S7. Line-loss recovery

Currently `MAX_CICLOS_SIN_DETECTAR` (100 cycles ≈ 200ms) stops the robot. A
bounded recovery usually scores better: on saturation, keep steering hard in the
direction of the last known error for a limited time, then stop. Check your
tournament rules first — some ban it — and cap the recovery duration so a robot
that has genuinely left the course doesn't drive into the audience.

Validate by carrying the robot off the line by hand and watching what it does.

### T-S8. Soft start

Ramp `SPEED` from 0 to target over ~100–200ms at launch. Reduces wheelspin,
which on a fresh course surface can throw the first metre of the run. Trivially
observable.

### T-S9. Autotune timeout

`AUTOTUNE_TIMEOUT_MS = 2000` against a comment reading 25s. Don't jump straight
to 25000 the week of a competition — that's up to 12× longer with the robot
oscillating on the track. Set an intermediate value (8–10s), and only if the
edge-abort from Tier 0 is confirmed working. If you can't confirm the abort,
leave the timeout alone and hand-tune; a failed autotune is recoverable, a robot
that drives off the table during one is not.

---

## Tier 3 — Do not ship before the tournament

Each of these is in the main plan and each needs data you don't have:

- **Gain scheduling of any kind.** Sign of `n` unknown. See Tier 0.
- **`dt`-normalising KD (§4.1).** Correct in principle, but it changes the
  meaning of every KD value you've ever tuned, by a factor you haven't measured.
  Post-tournament.
- **Derivative filtering.** Would reduce noise on the `position − last_position`
  delta, but adds phase lag to a loop whose margins you haven't characterised.
- **Curvature slowdown / feedforward (Phase 3C).** Real control-law change,
  needs its own validation pass.
- **EEPROM layout changes**, if not already flashed and verified. If the new
  layout is in, re-save with `W` and confirm read-back **now**, not at the venue.

---

## Freeze and pack

- **Freeze the build 48 hours before travel.** Tag it. No changes at the venue
  except gains, speed, and preset selection.
- Carry a laptop with the toolchain and a known-good `.hex` of the *previous*
  working firmware. If the new build misbehaves, you want a one-minute rollback,
  not a debugging session.
- Recalibrate under venue lighting, then run T-S3 preflight before every heat.
- Keep a paper log: profile used, peak `|posicion|`, line losses, result. That's
  your Phase 1 seed data.

## After the tournament

Return to `PLAN-speed-scheduling-v3_1.md` at Phase 0/1 with three things you
didn't have this week: real peak-error numbers across speeds from T-S1, a
partial T1.6 result from the §11.1 ladder, and a measured sense of where on the
course the robot actually fails. Fold the paper log into the Phase 1 results
table before running the relay sweep.
