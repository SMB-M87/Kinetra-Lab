# Linux beyond Arch and golden tests on every toolchain

Status: implemented · Branch: `ccr-915142f6-86mj8n` · Related: [Kinetra #32](https://github.com/SMB-M87/Kinetra/pull/32) and [#33](https://github.com/SMB-M87/Kinetra/pull/33), Kinetra design [Linux beyond Arch](../../external/Kinetra/docs/design/2026-11-22-linux-beyond-arch.md), roadmap items *Golden tests on every toolchain* and *Linux beyond Arch* (removed by this branch)

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
| One Linux job | Since 2026-11-23, one `linux` job with an `arch` and an `ubuntu` entry, each in its own container, and `checks` passes only when every entry passed, as in Kinetra's design *CI Linux matrix* (Kinetra #35) (the user, 2026-11-23) | No copied steps, so a fix to one distribution's job cannot miss the other; Ubuntu is required. In the container, `/usr/bin` holds Ubuntu's own tools without a path change |
| Ubuntu job | Same shape as Kinetra's: Ubuntu's own CMake and compiler (`/usr/bin` first), headless workflow, the `ci` preset with the engine's and the warning-free Lab modules' warnings as annotations instead of errors (`KINETRA_WARNINGS_AS_ERRORS=OFF`, the new `KINETRA_LAB_WARNING_FREE_AS_ERRORS=OFF`), every test including graphics, smoke runs and soak cases; format and docs only on Arch | Arch's pinned toolchain stays the reference for warnings (the user, 2026-11-22); review round 1 found that Lab's warning-free modules were still errors on Ubuntu |

## Design

- **Call sites.** Demo's `Agent` (heading from velocity, the heading home) and `Scene` (turn rate) and Dotio's `Vec2::get_direction` and lead angle use `kinetra::atan2` and `kinetra::asin`. With Kinetra's own uses in `signed_angle` and `tangent_steer`, every float `atan2` and `asin` in simulation logic then rounds correctly. i4sim has no call of its own; its RVO line moves through Kinetra.
- **Pointer.** `external/Kinetra` records the tip of Kinetra's `build/linux-beyond-arch` (#33), which contains #32 and, since review round 1, Kinetra `main` with the inspector overlay (#31) that Lab `main` needs.
- **Windows.** The `windows` and `windows-native` test presets first lost their name filter. CI's Windows job then passed Dotio's fingerprint and still failed the other five (see [verification](#verification)), so their exclusion came back and Dotio's stays removed. [Windows](../windows.md#tests) and the Dotio test's comment say so.
- **CI.** One job `linux` in `ci.yml` with entries `arch` and `ubuntu`, each in its own container, and the pointer-only skip condition. Every entry installs its own packages (Arch from the pinned snapshot; Ubuntu the README's apt set plus `git` and `ca-certificates`), prints the path and version of `gcc`, `cmake`, `ctest` and `ninja`, fetches the submodule and runs `cmake --workflow --preset headless`. `arch` also runs the format and docs checks and `cmake --workflow --preset ci`, with engine and warning-free-module warnings as errors. `ubuntu` runs `cmake --preset ci -DKINETRA_WARNINGS_AS_ERRORS=OFF -DKINETRA_LAB_WARNING_FREE_AS_ERRORS=OFF`, builds with `set -o pipefail` through `tee`, turns each distinct `warning: ` line into an annotation, and runs `ctest --preset ci`. The required `checks` waits for `linux` and passes when every entry passed, or when `changes` succeeded and skipped the build. Until review round 2 this was a separate job `ubuntu` on the runner itself, with `/usr/bin` put first on the path.
- **Warnings option.** The root `CMakeLists.txt` has `KINETRA_LAB_WARNING_FREE_AS_ERRORS` (default `ON`): with it off, the warning-free modules (Demo, i4sim, Dotio, the capture tool) get the same warning mode as the rest of Lab instead of `AS_ERRORS`.
- **Docs.** The README's quick start gives the three families' commands and says which CI tests. Demo's README no longer says the recorded lines depend on the compiler.

## Affected areas

`external/Kinetra`, `labs/demo/src/world/agent.cpp`, `labs/demo/src/world/scene.cpp`, `labs/dotio/src/math/vector.cpp`, `labs/dotio/tests/dotio_tests.cpp` (comment), `CMakePresets.json` (Windows test filters), the root `CMakeLists.txt` (`KINETRA_LAB_WARNING_FREE_AS_ERRORS`), `.github/workflows/ci.yml`, `AGENTS.md` (warnings), `README.md`, `docs/windows.md`, `labs/demo/README.md`, `labs/dotio/README.md`, the [Demo](2026-11-21-demo-restructuring.md#open-points) and [Dotio](2026-11-21-dotio-restructuring.md#open-points) restructurings' open points, the [review lessons](../review-lessons.md), this document, the [design index](README.md#index), the [roadmap](../roadmap.md).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| The five fingerprints hold on Ubuntu with Arch's recorded values | The existing Demo crowd characterization and i4sim golden cases, run on Ubuntu 24.04 locally (they fail on `main` there) and in the new `ubuntu` job | Full |
| Nothing changes on Arch | Every recorded line, in CI's `checks` job | Full |
| Dotio's fingerprint and the five others hold on Windows | CI's `windows` job with the exclusions removed | Full |
| Lab builds and passes on Ubuntu LTS, graphics, smoke and soak cases included | The new `ubuntu` job | Full |

## Review

Self-review of the branch diff against `main`:

- Every float `atan2` and `asin` in Lab's logic now goes through Kinetra (`grep -rnE "\b(std::)?(atan2|asin)f?\("` over `labs/` finds only a comment in Dotio's header and a test in `terrain_tests.cpp` that computes a turn angle for an assertion with a tolerance). i4sim's logic has no call of its own.
- No recorded value changed: the fix keeps Arch's results, and every fingerprint was compared against its existing expected value, not re-recorded.
- The Windows step was a trial: CI's Windows job passed Dotio's fingerprint and failed the other five, so their exclusion came back in a new commit, with the values in [Windows](../windows.md#tests). The README and Demo's README, which the first version made say *Windows too*, now say what holds where.
- Copies: "skipped on Windows" and "depend on the compiler" were stated in [Windows](../windows.md#tests), Demo's README (twice), the Dotio test's comment, two open points of the [Demo](2026-11-21-demo-restructuring.md#open-points) and [Dotio](2026-11-21-dotio-restructuring.md#open-points) restructurings, and the Dotio playtest item of the roadmap, and all now say the same. The Full level's list of CI checks changed in Lab's AGENTS.md and, in Kinetra #33, in AGENTS.md, testing and the `verify` skill; the Kinetra testing page's copy was found during this review and fixed on #33.
- The `ubuntu` job used the same condition as `checks` and `windows`, so a pointer-only pull request skipped it. Since the `linux` matrix, `checks` passes on a skip only when `changes` succeeded and said to skip; a failed or cancelled `linux` run fails it.
- [Windows](../windows.md#tests) used to say five cases were excluded while the presets excluded six (Dotio's too); it now names the five that stay excluded and says that Dotio's runs.

Agent review of `9b86757` (Fix first), round 1:

| Finding | Resolution |
| --- | --- |
| Blocking: Lab `main` moved its pointer to Kinetra #31 after this pull request's CI, the Kinetra branch moved on with #32's and #33's fixes, and the roadmap and design index conflicted | Kinetra `main` merged into both Kinetra branches, the pointer follows `build/linux-beyond-arch` (at `a803e2b`, then at its tip after #32's and #33's second round, which changed only docs, a contract comment and its formatting), and Lab `main` merged here with both roadmap items gone and every index row kept (`e0a8920`) |
| Should fix: the `ubuntu` job ran the runner image's CMake 3.31.6 | `/usr/bin` first on the path and a *Tool versions* step, as Kinetra #33 does |
| Should fix: the warning-free Lab modules stayed errors on Ubuntu | New option `KINETRA_LAB_WARNING_FREE_AS_ERRORS` (default on); the `ubuntu` job turns it off and shows the build's warnings as annotations. The README and `ci.yml` say so |
| Should fix: two copies of "skipped on Windows" were missed | Dotio's README says the fingerprint runs on Linux and Windows and why; the Dotio restructuring's open point marks the decision settled |
| Nits: where to record Demo's lines; "every C library" without the exception; includes | Demo's README says to record on Linux, never on Windows; "in practice" with a pointer to Kinetra's `math.h` in Demo's README, Windows tests and the Dotio test; `agent.cpp` and `scene.cpp` include `<kinetra/simulation/math.h>` |
| Two lessons proposed | Added to the [review lessons](../review-lessons.md) |

Agent review of `d7836e9` (Fix first), round 2: every round-1 thread accepted and resolved. New points:

| Finding | Resolution |
| --- | --- |
| Should fix: `AGENTS.md` still said engine warnings are errors in CI and told to add a warning-free module with a literal `AS_ERRORS` | Names the runs that treat warnings as errors and says to add a module with `${warning_free_mode}` |
| Should fix: the *Design* bullet, *Affected areas* and the description did not describe round 1's changes, and the *Summary* said "every C library" | All three rewritten, together with the user's change of the same day to one `linux` matrix job (Kinetra #35) |
| Nit: "warnings are errors only there" left out Windows | The `ci.yml` header names Arch and the `windows` job |
| Nit: the Kinetra include sat in `agent.cpp`'s standard-library block | A block of its own before the standard library |
| Lesson proposed | Added to the [review lessons](../review-lessons.md) |

## Verification

On Ubuntu 24.04 (GCC 13.3, glibc 2.39), with Kinetra at `build/linux-beyond-arch`:

| Command | Result |
| --- | --- |
| `ctest --preset ci -j4 -LE soak` on `main` | 311 of 316: the five fingerprints in the [problem](#problem) fail |
| The same with an `LD_PRELOAD` library of correctly rounded `atan2f` and `asinf` | 316 of 316 |
| `cmake --build --preset ci` and `ctest --preset ci -j4` on this branch | No build warning or error; 321 of 321, the soak, graphics and smoke cases included |
| `cmake --workflow --preset headless` | 259 of 259 |
| `clang-format --dry-run` (version 18) on the changed sources | Clean; CI checks with the pinned version 23 |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |

After review round 1, with Lab `main` merged and Kinetra at `a803e2b`: `cmake --preset ci`, then the Demo, crowd characterization, Dotio and i4sim golden targets build without a warning and `ctest --preset ci -R '^kinetra_(demo:|demo_crowd_characterization|dotio:|i4sim_golden)'` passes 89 of 89. Configuring with `KINETRA_LAB_WARNINGS=ON` gives `-Werror` to 8 of 8 Demo sources by default and to none with `KINETRA_LAB_WARNING_FREE_AS_ERRORS=OFF`.

The pull request's CI is the Full level: `checks` on the pinned Arch toolchain, the new `ubuntu` job and `windows`.

**Windows trial.** CI's `windows` job at `052adb8`, with no exclusion: 292 of 297. Dotio's fingerprint and every other golden and pressure case passed; the four Demo crowd characterization cases and i4sim's RVO case failed. i4sim's distance now matches (17582.890 mm, against 17582.889 mm before), its trajectory hash does not; the Demo lines also differ from Ubuntu's old ones. So Kinetra's `atan2` and `asin` closed part of the Windows difference, and another C-runtime function closes the rest. The exclusion of these five came back; the values are in [Windows](../windows.md#tests).

## Pull request

Title: `fix: golden fingerprints on every toolchain, Ubuntu CI and Linux per distribution`

**Summary.** Lab's trajectory fingerprints held only on CI's Arch toolchain: they failed on Ubuntu and were skipped on Windows. The cause is the C library's float `atan2f` and `asinf`, not GCC 13. With Kinetra's `atan2` and `asin`, which round the same on every C library in practice ([Kinetra #32](https://github.com/SMB-M87/Kinetra/pull/32); the rare exception is in its `math.h`), the recorded values hold unchanged on Ubuntu; this pull request moves Lab's calls to them, runs Dotio's fingerprint on Windows again, turns CI's Linux build into one `linux` job with an Arch and an Ubuntu LTS container, both required, and documents the dependencies per distribution.

**Changes.**
- Demo (`Agent`, `Scene`) and Dotio (`Vec2`) call `kinetra::atan2` and `kinetra::asin`.
- `CMakePresets.json`: the Windows test presets no longer exclude Dotio's fingerprint; the five other fingerprints stay excluded, because Windows still differs (tried in this pull request, values in `docs/windows.md`).
- CI: one job `linux` with entries `arch` (the pinned reference: format, docs, warnings as errors) and `ubuntu` (an `ubuntu:24.04` container with Ubuntu's own tools, warnings as annotations); the required `checks` passes when both passed or the build was rightly skipped.
- CMake: option `KINETRA_LAB_WARNING_FREE_AS_ERRORS` (default `ON`), which the Ubuntu run turns off.
- Docs: dependency commands per distribution family in the README, the Full level's CI checks and the warning rule in AGENTS.md, Windows tests, Demo's and Dotio's READMEs, the Dotio test comment, three older open points, the roadmap (both items removed, *Golden tests on Windows* added, *Linux release* unblocked), three review lessons.
- `external/Kinetra` at Kinetra's `build/linux-beyond-arch` (#33 on top of #32).

**Design.** `docs/design/2026-11-22-linux-beyond-arch.md`, and Kinetra's design of the same name.

**Verification.** On Ubuntu 24.04 the five fingerprints that fail on `main` pass with Arch's recorded values; 321 of 321 with the soak, graphics and smoke cases; headless 259 of 259; after merging Lab `main`, 89 of 89 Demo, Dotio and golden cases. `actionlint` passes on `ci.yml`. The pull request's CI on Arch, Ubuntu and Windows is the Full level.

**Review notes.** No expected value changed. The Windows step was a trial the user approved: Dotio's fingerprint passed; the Demo crowd cases and i4sim's RVO case still differ, so their exclusion came back, with the values recorded. The Dotio playtest item listed "the golden fingerprint skipped on Windows" among the decisions for the user's review; that decision is gone. The Fedora command is not checked against Fedora's repositories.

**Dependencies and merge order.** Kinetra [#32](https://github.com/SMB-M87/Kinetra/pull/32) and [#33](https://github.com/SMB-M87/Kinetra/pull/33) are merged; the pointer is at #33's head `4b91463`, which `main` contains. After this pull request, a pointer-only pull request to Kinetra `main`. Kinetra [#35](https://github.com/SMB-M87/Kinetra/pull/35) makes the same matrix change in Kinetra and is independent. Changes CI and the agent rules, so it waits for the user.

**Follow-up.** *Golden tests on Windows* (roadmap) for the five cases still excluded. *Linux release* is unblocked. The Fedora check stays in Kinetra's *Linux support follow-ups*; a Fedora matrix entry can follow it.

## Open points

- Five fingerprints still differ on Windows; which C-runtime function causes it, and whether to route it through Kinetra too, is the roadmap item *Golden tests on Windows*. Doing so for `sinf` or `cosf` would move the recorded lines on Linux as well, because glibc's versions of those are not correctly rounded either: the user decides.
- The Fedora command comes from Kinetra's development page, whose package names this session could not check against Fedora.
