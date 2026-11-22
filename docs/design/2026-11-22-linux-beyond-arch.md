# Linux beyond Arch and golden tests on every toolchain

Status: accepted · Branch: `ccr-915142f6-86mj8n` · Related: [Kinetra #32](https://github.com/SMB-M87/Kinetra/pull/32) and [#33](https://github.com/SMB-M87/Kinetra/pull/33), Kinetra design [Linux beyond Arch](../../external/Kinetra/docs/design/2026-11-22-linux-beyond-arch.md), roadmap items *Golden tests on every toolchain* and *Linux beyond Arch*

Kinetra-Lab's trajectory fingerprints held only on CI's pinned Arch toolchain, so a local run on any other Linux failed, they were skipped on Windows, and Linux beyond Arch could not start. The cause turned out to be two float functions of the C library, not the compiler. With Kinetra's new `atan2` and `asin`, which round the same on every C library, the recorded fingerprints hold on Ubuntu unchanged; this branch moves Lab's own calls to them, tries the fingerprints on Windows again, adds an Ubuntu LTS job to CI and documents the dependencies per distribution.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

On Ubuntu 24.04 (the cloud sessions' system: GCC 13.3, Clang 18.1, glibc 2.39), `ctest --preset ci -LE soak` on `main` fails five of 316 cases. The values, against those recorded on Arch:

| Case | Ubuntu 24.04 | Recorded on Arch |
| --- | --- | --- |
| Demo, dense idle crowd, steering | `avoidance_limited=2953 trajectory=13399899961ceab5` | `avoidance_limited=3047 trajectory=25eff655f7a75f9e` |
| Demo, dense idle crowd, route | `avoidance_limited=722 trajectory=7a498b80d38c2bf6` | `avoidance_limited=722 trajectory=17080ac73d99bce9` |
| Demo, streamed chunks, steering | `avoidance_limited=2532 trajectory=741f388a69298f34` | `avoidance_limited=2623 trajectory=377eeb3999727f88` |
| Demo, streamed chunks, route | `avoidance_limited=2764 trajectory=ea74314d22af67c3` | `avoidance_limited=2756 trajectory=37e262315f729cfb` |
| i4sim, experimental RVO profile | `distance_mm=17582.889 trajectory=6db01fe731e12242` | `distance_mm=17582.890 trajectory=bc246a58724f4083` |

Agent, body and contact counts agree; the trajectories and the counts that follow them do not. The same five cases are excluded on Windows ([Windows tests](../windows.md#tests)), where the i4sim line also ends at 17582.889 mm, and so is Dotio's golden fingerprint (`CMakePresets.json`).

**Not the compiler.** GCC 13.3 Release, GCC 13.3 Debug and Clang 18.1 Release give bit-identical lines on Ubuntu, so the roadmap's "Ubuntu's GCC 13" was the wrong suspect.

**The C library.** glibc 2.41 replaced `atan2f` and `asinf` with correctly rounded implementations (CORE-MATH). An `LD_PRELOAD` library that replaces only these two with `(float)atan2((double)y, (double)x)` and `(float)asin((double)x)` makes all five cases pass on Ubuntu with Arch's recorded values, and Lab's other 311 cases and Kinetra's 278 stay green; `atan2f` alone fixes the four Demo cases. Correctly rounded `sinf`, `cosf`, `expf` or `powf` break the cases again, so those already agree between the two glibc versions. The Kinetra design document has the details.

Linux beyond Arch also needs CI and docs: the [README](../../README.md#quick-start) gives a `pacman` command and one Debian package name, and CI runs only the pinned Arch image and Windows.

## Goals and non-goals

Goals:

- Lab's simulation logic calls `kinetra::atan2` and `kinetra::asin` instead of the C library's float versions, so its fingerprints hold on every glibc with the values recorded on Arch.
- The fingerprints skipped on Windows run there again, if CI's Windows job passes them.
- A CI job on Ubuntu LTS that builds and tests Lab next to the pinned Arch job, which stays the reference for warnings and formatting.
- Dependency commands per distribution family in the README, and docs that name what CI tests.

Non-goals:

- A tolerance, totals instead of fingerprints, or a fingerprint per toolchain: the user chose Kinetra's functions instead (2026-11-22).
- Other C-library functions (`sinf`, `cosf`, `expf`, `powf`, `hypot`). If Windows still differs because of one of them, the skip stays for that case and the difference is recorded.
- A Fedora CI job, and a Linux release (its own roadmap item, which this unblocks).

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| How golden tests hold on every toolchain | Kinetra's `atan2` and `asin`, computed in double and rounded once to float; fingerprints stay strict (the user, 2026-11-22) | The measured difference is exactly these two functions; a tolerance or totals would weaken every fingerprint |
| Windows | Remove the six exclusions from both Windows test presets and let CI's Windows job decide; restore a case's exclusion only if it still fails, with its values recorded (the user, 2026-11-22) | Windows' i4sim line differs in the same digit as Ubuntu's, a hint that the cause is the same |
| Order | One stack: Kinetra #32 (functions), Kinetra #33 (Ubuntu job), then this pull request (the user, 2026-11-22) | Lab's Ubuntu job needs both |
| One Lab pull request | The golden fix and the Ubuntu job share this branch, in separate commits | This session may push only its assigned Lab branch; the pull request changes CI and waits for the user anyway |
| Ubuntu job | Same shape as Kinetra's: headless workflow, the `ci` preset with engine warnings reported instead of errors, every test including graphics, smoke runs and soak cases; format and docs only on Arch | Arch's pinned toolchain stays the reference (the user, 2026-11-22) |

## Design

- **Call sites.** Demo's `Agent` (heading from velocity, the heading home) and `Scene` (turn rate) and Dotio's `Vec2::get_direction` and lead angle use `kinetra::atan2` and `kinetra::asin`. With Kinetra's own uses in `signed_angle` and `tangent_steer`, every float `atan2` and `asin` in simulation logic then rounds correctly. i4sim has no call of its own; its RVO line moves through Kinetra.
- **Pointer.** `external/Kinetra` records the tip of Kinetra's `build/linux-beyond-arch` (#33), which contains #32.
- **Windows.** The `windows` and `windows-native` test presets lose their name filter; `windows-native` keeps excluding the `graphics` label. [Windows](../windows.md#tests) and the Dotio test's comment say so.
- **CI.** A job `ubuntu` in `ci.yml` on `ubuntu-24.04`, with the same skip condition as `checks` and `windows`: the Debian and Ubuntu packages of the README, then `cmake --workflow --preset headless`, `cmake --preset ci -DKINETRA_WARNINGS_AS_ERRORS=OFF`, its build and `ctest --preset ci`.
- **Docs.** The README's quick start gives the three families' commands and says which CI tests. Demo's README no longer says the recorded lines depend on the compiler.

## Affected areas

`external/Kinetra`, `labs/demo/src/world/agent.cpp`, `labs/demo/src/world/scene.cpp`, `labs/dotio/src/math/vector.cpp`, `labs/dotio/tests/dotio_tests.cpp` (comment), `CMakePresets.json` (Windows test filters), `.github/workflows/ci.yml`, `README.md`, `docs/windows.md`, `labs/demo/README.md`, this document, the [design index](README.md#index), the [roadmap](../roadmap.md).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| The five fingerprints hold on Ubuntu with Arch's recorded values | The existing Demo crowd characterization and i4sim golden cases, run on Ubuntu 24.04 locally (they fail on `main` there) and in the new `ubuntu` job | Full |
| Nothing changes on Arch | Every recorded line, in CI's `checks` job | Full |
| Dotio's fingerprint and the five others hold on Windows | CI's `windows` job with the exclusions removed | Full |
| Lab builds and passes on Ubuntu LTS, graphics, smoke and soak cases included | The new `ubuntu` job | Full |

## Review

Filled in after the self-review.

## Verification

Filled in after verification.

## Pull request

Filled in at the end.

## Open points

- The Fedora command comes from Kinetra's development page, whose package names this session could not check against Fedora.
