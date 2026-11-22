# One prioritized roadmap

Status: implemented · Branch: `ccr-cc3e854a-v10cnx` · Related: [roadmap](../roadmap.md), [quality roadmap](2026-11-18-quality-roadmap.md) (closed here), [Kinetra stacked pull requests](../../external/Kinetra/docs/design/2026-11-21-stacked-pull-requests.md), [Kinetra roadmap priorities](../../external/Kinetra/docs/design/2026-11-21-roadmap-priorities.md)

The user wants one plan that everyone can read: no more references to the quality roadmap, everything still open or waiting for a decision on the roadmap, in order of priority, and the order discussed before new work starts. They also brought new wishes and questions, and asked for a standing rule to work in stacked pull requests. This change closes the quality roadmap, rebuilds the roadmap from every open point, and records the agent's assessment of each new wish; the stacking rule and the engine half of the roadmap are two stacked Kinetra pull requests.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Assessments](#assessments)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

- Two plans: the roadmap and the quality roadmap, which AGENTS.md, the docs index, the README, the Demo and i4sim READMEs and the roadmap itself still cite for scope. Phase 8's scope lived only in the quality roadmap.
- Stale items: *Demo's contact tests to Kinetra* is done (Lab #64, Kinetra #19), the demo restructuring still says step 4 waits for Kinetra access, and the quality roadmap's status table still shows phases 5 and 9 in review. The capture README still says Dotio is not seeded, which phase 6 changed.
- Open points of phases 5 to 7 that wait for the user were not all on the roadmap: phase 7's decisions taken by the agent, the RECOVERY corner detour, smoother Dotio pursuit and the timing header (only in Kinetra's roadmap).
- New wishes of 2026-11-21 with no item: an agent inspector overlay, stall detection with a temporary motion fallback in i4sim, a motion lab, Linux beyond Arch and a Linux release, a pull request review skill, testing and tooling improvements, a critical look at performance, and the question whether the docs could move to the GitHub wiki.

## Goals and non-goals

- Goal: the roadmap is the only plan and holds every open item, self-contained, in order of priority.
- Goal: the quality roadmap is closed as a record; living pages no longer cite it for planning.
- Goal: the agent's view on every new wish is recorded, so the discussion of the order starts from it.
- Goal: the stacking rule in the agent rules (Kinetra, shared by Lab).
- Non-goal: starting any item. Non-goal: editing earlier design documents' links; they are records.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Quality roadmap | Closed as `implemented`, kept as the record; its status table corrected | The user's choice to end it as a plan; its evidence and decisions stay findable from older design documents |
| Order of Next | The user's decisions and checks first, then process and tooling, then the inspector overlay and the performance baseline | Proposed by the *agent*, to discuss with the user. Decisions are short and unblock work; the review skill, cloud setup, golden tests and graphics tests in CI make every later pull request cheaper to verify |
| MAKE WAY and the motion fallback | One decision item, one Later item for the work | Both answer what a stalled mover may do; one design session settles both |
| Blocked items | In Later with *Depends on*, even when important | The roadmap format: Next holds only what nothing blocks |
| Shared passing mechanism | After the stall remedies | Share what i4sim has proven, as the quality roadmap decided for passing |
| Stacking in Kinetra | Two stacked pull requests: the rule, then the roadmap | They edit the same index and skills overview, and Lab records one pointer that needs both |
| Lab branch | One branch, `ccr-cc3e854a-v10cnx` | The session may push only this Lab branch |
| CI during the archive outage | `ci.yml` falls back to the current Arch packages when the Arch Linux Archive does not answer, with a *Toolchain not pinned* warning; a separate commit on this branch. `release.yml` stays pinned | From 18:11 UTC on 2026-11-21 `archive.archlinux.org` did not answer GitHub's runners, and every `checks` job failed in its first step, re-runs included. The user suggested the current, non-archived Arch as fallback. A release should come from the pinned toolchain, so it waits for the archive ([Kinetra design](../../external/Kinetra/docs/design/2026-11-21-ci-mirror-fallback.md)) |

## Design

The roadmap keeps its four sections. Every open point that waits for someone is an item, with its source under *Where*:

| Source | Items |
| --- | --- |
| Quality roadmap, phase 8 and open points | *Shared passing mechanism* (scope now in the item), *i4sim session behavior*, *Warnings as errors for the launcher and shared code* |
| Phase 5 (Demo) | *Demo's help overlay*, *Golden tests on every toolchain* |
| Phase 6 (Dotio) | *Dotio playtest and phase 6's decisions*, *Dotio on `kinetra::Vec2`*, *Smoother pursuit in Dotio*, *Timing header out of `runtime/`*, *Reproducible Dotio captures and a new tour video* |
| Phase 7 (i4sim) | *Decide on making way and a temporary motion fallback*, *Phase 7's decisions*, *Stall detection and remedies in i4sim*, *RECOVERY detour past a corner* |
| Phase 9 and drafts | *Windows zip on a desktop*, *Frame pacing in WSLg* |
| Module open points | *Menu rows by label in tests and scripts*, *One i4sim session frame helper*, *Launcher `--seed`*, *Capture on Kinetra's frame read-back* |
| The user's wishes of 2026-11-21 | *Pull request review skill*, *Agent inspector overlay*, *Performance baseline and hotspots*, *Linux beyond Arch*, *Linux release*, ideas *Motion lab* and *Documentation as a site* |
| The agent's tooling proposals | *Ready-to-build cloud sessions*, *Graphics tests in CI*, *Impact-based verification script*, *More checks in CI*, *Blueprint loader fuzzing* |

Open points that nobody intends to act on stay on their pages, as the format asks: Improved's jostling (a profile limit), Dotio's `get_signed` and `Tracker` quirks from the original game, finite district integers.

## Assessments

The agent's view on the new wishes, as the starting point of the discussion.

- **Docs on the GitHub wiki.** Possible, but functions would be lost. A wiki is a separate Git repository without pull requests, CI or branch protection: design documents could no longer merge together with their code, `check_docs.py` would not run, relative links to code and between Kinetra and Lab would break, and agents read the docs from their checkout through AGENTS.md. Advice: keep `docs/` as the source; publish a read-only copy (wiki or Pages) on every merge if a browsable site is wanted.
- **Linux beyond Arch.** Feasible and mostly documentation and CI: per-distribution dependency commands and a CI job on Ubuntu LTS next to the pinned Arch job, which stays the reference. The one real blocker is in the tests: five golden cases hold only for CI's Arch toolchain and fail with Ubuntu's GCC 13. Fix those first.
- **Linux release.** Not too much: the release workflow and packaging script exist for Windows. A second job builds on an older distribution, so that the C library runs on newer ones, and packs the executables, SDL2, the blueprints and the licenses as a tarball or AppImage; OpenGL comes from the system. It follows *Linux beyond Arch*, which proves a non-Arch build.
- **Agent inspector overlay.** Fits the existing boundaries: Kinetra owns overlays (layout, drawing, input capture), applications supply content. i4sim and Demo already select an agent. Rows as headless values make it testable, and the same rows printed from the command line give agents and tests the user's view without a window.
- **Stall detection and a temporary motion fallback.** The documented standstill near a destination is a mover sealed in by parked neighbors; no motion type gets it out while they stay parked, so it needs making way. A temporary switch of motion type fits other stalls: two movers that hold each other in a standoff, or jostling beside parked movers. Detection first, without behavior change, then each remedy as an optional policy, measured with the 48-run harness, and visible in the inspector.
- **Motion lab.** A crowd lab with classic scenarios, steering models side by side, heat maps and live measurements; it doubles as the visual test bed of the shared passing mechanism, so it comes after it.
- **Testing and tooling.** In order of value for an agent: a pull request review loop; cloud sessions that can build at once (a setup script with SDL2, nlohmann JSON, ccache and CI's clang-format version); golden tests that hold on every toolchain; graphics tests in CI with software Mesa; one script that turns changed paths into the verification commands; later sanitizers, clang-tidy and coverage in CI, and fuzzing of the blueprint loader.
- **Performance.** Measure before optimizing: a profile of the existing benchmarks and a hotspot list, then one optimization per pull request with a measurement before and after and unchanged golden lines. Candidates are named in the item.
- **Pull request review skill.** Possible. The user's GitHub account cannot approve or request changes on its own pull request, so agent reviews are comment reviews, which matches "never merge". The open question is how the author's session hears about a review: a cloud session can subscribe to a pull request's activity; locally it is a request or `/loop`.

## Affected areas

`docs/roadmap.md`, `docs/design/2026-11-18-quality-roadmap.md`, `docs/design/2026-11-21-demo-restructuring.md`, this document, the design index, `AGENTS.md`, `README.md`, `docs/README.md`, `labs/demo/README.md`, `labs/i4sim/README.md`, `labs/i4sim/tests/README.md`, `tools/capture/README.md`, and the `external/Kinetra` pointer. The CI commit changes the install step of `.github/workflows/ci.yml`.

## Test plan

Documentation, the CI install step, and a pointer to a Kinetra commit without engine sources: the Docs level in both repositories, plus each pull request's CI. The pointer row of the verification table asks for the Full level for engine sources; here the Kinetra diff (`git -C external/Kinetra diff --stat 8896a19 3ac0c81`) holds only `.md` files and Kinetra's own `ci.yml`, which Lab's build does not use, so no build changes.

## Review

- Every item checked against its source, and every Kinetra item that Lab depends on exists in Kinetra's roadmap with the same title.
- Searched for `quality roadmap` in living pages: what remains are design documents of earlier branches (records), the closed page itself, one evidence link to its predictive passing baseline in the i4sim tests README, and two code comments (`CMakeLists.txt`, `ci.yml`) that speak of a module's "roadmap phase"; changing those would raise this change to the Full level.

## Verification

- `python3 external/Kinetra/tools/check_docs.py` in Lab: all links resolve.
- `python3 tools/check_docs.py` in `external/Kinetra` on `docs/stacked-pull-requests` and on `docs/roadmap-priorities`: all links resolve.
- `ci.yml` parses as YAML and its install step as `sh` (`sh -n`); the fallback itself is verified by this pull request's CI, because the agent's session cannot reach any Arch host.
- #65 merged with the pointer at Kinetra 3ac0c81, the top of the stack. A follow-up pointer pull request records c03a36d, the merge commit of Kinetra #21 on Kinetra `main`; `git -C external/Kinetra diff --stat 3ac0c81 c03a36d` is empty, so this verification stands.

## Pull request

**docs(roadmap): one prioritized roadmap; the quality roadmap is closed**

- **Summary:** the roadmap is the only plan. It holds everything the quality roadmap and phases 5 to 7 left open and the user's new wishes, in a proposed order of priority. Kinetra gains the rule to work in stacked pull requests.
- **Changes:** roadmap rebuilt (user decisions first, then process and tooling, inspector overlay, performance baseline; stall remedies, shared passing, Linux and the Linux release under Later); quality roadmap closed and its status corrected; references removed from AGENTS.md, README, docs index, Demo, i4sim and capture READMEs; stale Kinetra-access note and capture determinism text fixed; submodule pointer to Kinetra `docs/roadmap-priorities`.
- **Design:** this document, with the agent's assessment of every new wish.
- **Verification:** Docs level in both repositories, green.
- **Review notes:** the order in *Next* is the agent's proposal and is discussed with the user; changes follow in new commits.
- **Dependencies and merge order:** Kinetra `docs/stacked-pull-requests`, then Kinetra `docs/roadmap-priorities` (stacked), both with a merge commit; then move this branch's pointer to the Kinetra merge commit and merge this one.
- **Follow-up:** the first items of *Next*.

## Open points

- The user confirms or changes the order of *Next* and *Later*.
