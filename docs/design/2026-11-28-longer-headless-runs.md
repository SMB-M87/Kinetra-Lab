# Longer headless runs across the labs

Status: implemented · Branch: `test/longer-headless-runs`

Demo, Dotio and i4sim can run their simulations headless at the speed of the test machine. The sustained tests will each exercise at least 100,000 simulation ticks and cover the highest requested clock rate of 1,000,000 UPS.

## Table of contents

- [Problem](#problem)
- [Decisions](#decisions)
- [Test plan](#test-plan)
- [Verification](#verification)
- [Review](#review)
- [Pull request](#pull-request)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

i4sim's sustained factory tests stop at 50,000 ticks. Demo's crowd tests stop after 7,200 fixed steps, and Dotio's behavior benchmark defaults to 10,800 while CTest runs only 600. These horizons limit coverage of delayed problems such as crowd dispersal and factory stalls. The i4sim motion benchmark already defaults to 1,000,000 ticks and reports measured UPS.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Sustained workload | 100,000 ticks per run | Doubles the previous i4sim window and gives Demo and Dotio the same minimum coverage |
| Timing | Advance headless without real-time pacing at each lab's existing fixed simulation step | Preserves physics, input order and seeded replay while using the available CPU throughput |
| Clock coverage | Exercise 1,000,000 requested UPS using synthetic elapsed time and verify the number of updates | Covers the highest rate without introducing a wall-clock performance requirement in CI |
| Focused cases | Keep the established short Demo dense-crowd reproductions, Dotio smoke and recorded golden checkpoints | Maintains fast feedback and preserves the meaning of recorded fingerprints |

Extend Demo's standing-player steering and route cases, add a separately labelled Dotio run at all difficulties, and extend i4sim's factory recovery, production, parking and make-way runs. Share the sustained tick budget through the Lab test support target. Accept both `--seconds` for historical Dotio comparisons and `--ticks` for exact budgets. Use the existing million-tick i4sim benchmark for performance measurements.

## Test plan

- Demo checks the retained active population at the existing 7,200-tick checkpoint and at 100,000 ticks in both wander modes. Each clock advance must produce exactly one tick.
- Dotio runs a fixed seed for 100,000 ticks at each difficulty, with exactly one tick per scripted input. Compare `--ticks 600` with `--seconds 10` and reject invalid durations. Verify that the long run reports its budget and measured UPS.
- i4sim checks that both large factories recover and complete products, the test environment progresses beyond the old checkpoint, and the parking replay stays deterministic with bounded motion and no contacts. Verify the actual tick count and compare a timed Reciprocal update at the maximum UPS with a normal single step.
- Run the affected Release suites, an explicit million-tick i4sim batch and the motion benchmark on the large factory. Verify the edited documentation and formatting. The pull request's CI runs the platform and graphics suites.

## Verification

- Configured the Release preset with `KINETRA_LAB_WARNINGS=ON` and built the affected Lab targets against the recorded Kinetra commit `f2e55eb`.
- The targeted `ctest --preset release -j2 --output-on-failure` run passed all 351 selected Demo, Dotio, i4sim and shared cases. Demo's two 100,000-tick cases took 43.66 and 40.60 seconds. Dotio ran 300,000 ticks across its three difficulties at 7,663 UPS.
- Dotio's `--ticks 600` and `--seconds 10` produced identical statistics and fingerprints at seed 1. Zero, fractional and out-of-range tick counts and invalid elapsed durations were rejected.
- `kinetra_i4sim --blueprint labs/i4sim/tests/test-environment.json --ticks 1000000 --ups 1000000 --seed 42` completed 1,000,000 ticks at 216,249 UPS. The default-length motion benchmark on `8640x4800` at seed 42 measured 6,950 UPS. The benchmark and the tests shared the runner, so these figures describe the observed workloads.
- The documentation link and copy checks, pinned clang-format check for the edited C++ files, and `git diff --check` were run. Retained references to 50,000 ticks describe historical benchmarks or the original production checkpoint. The Ubuntu and native Windows CI runs provide platform coverage.

## Review

Reviewed the changed tests and their normal entry points. Demo and Dotio verify one actual update per synthetic interval at the highest clock rate. i4sim verifies the actual length of each sustained factory run and preserves the 50,000-tick production thresholds in the test environment. Recorded fingerprints and the earlier benchmark recipes remain available for comparison.

## Pull request

`test(labs): extend headless simulation workloads`

Use a shared 100,000-tick budget for sustained Demo, Dotio and i4sim tests. Exercise the highest requested clock rate in each lab. Dotio now accepts `--ticks` and reports elapsed time and measured UPS. Keep the i4sim motion benchmark's existing one-million-tick default.

Local Release verification passed all 351 selected cases. The million-tick i4sim batch and large-layout benchmark also completed. See the [design and verification](2026-11-28-longer-headless-runs.md#verification). The change is contained in Lab.
