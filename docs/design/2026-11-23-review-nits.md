# Code nits from the last review rounds

Status: implemented · Branch: `ccr-ad716caf-1sr52q-nits` in both repositories (set by the session environment) · Related: [Kinetra#39](https://github.com/SMB-M87/Kinetra/pull/39), Kinetra#37's round 7, Kinetra-Lab#83's round 5

The last review rounds of Kinetra#37 and Kinetra-Lab#83 ended *Ready to merge* with nits that were left for after the merge. The docs nits went into Kinetra#38. This change carries out the three that touch CI or a script, so they could not go into a docs-only pull request: one in Kinetra, two in Lab, and Lab's copy of Kinetra's.

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

- **Docs check placement.** Since Kinetra#37 and #83, the `changes` job runs the docs check when it skips the build. Kinetra's `ci.yml` header and the step table in `docs/testing.md` still placed it only on `arch` (Kinetra#37, round 7, point 1), Lab's `ci.yml` header said the same (`.github/workflows/ci.yml:10`), and Lab's `README.md` said that `checks` passes only when every distribution passed (`README.md:148`).
- **Decision-script warning.** In Lab's `changes` job, `git show "$BASE:tools/release/needs_build.sh"` fails when the file is missing at the base tip, but also when the blobless checkout cannot fetch it. The warning *No decision script* blamed a missing file in both cases (#83, round 5, point 1). The run still builds in full.
- **Tour output folder.** `capture_tour.sh` created the output folder only after the render of about five minutes. When `mkdir` failed, `set -e` stopped the script there, and the `EXIT` trap deleted the rendered video with the work folder (#83, round 5, point 2). An output folder that exists but cannot be written lost the video the same way, at the final `mv`, and so did a full disk during that `mv`, which copied across file systems from `mktemp -d`'s folder under `/tmp`.

## Goals and non-goals

- Goal: every page and header that says where the docs check runs names the `changes` job.
- Goal: the warning names both causes it can have.
- Goal: an output folder that cannot be made or written stops the tour before the render.
- Non-goal: any change to what CI runs or decides, or to the video.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Separate pull requests from the docs nits | Yes, chosen by the user on 2026-11-23 | They touch `ci.yml` or a script, so they need full CI and an agent review; the docs nits and the rules stayed docs-only in Kinetra#38 and Kinetra-Lab#86 |
| One design document | This one, in Lab, which holds most of the change | Kinetra's [rule](../../external/Kinetra/AGENTS.md#development-cycle): one document for a change across both repositories |
| The warning | Reworded, not split into two checks | Telling a missing file from a failed fetch in a blobless clone needs another fetch that can fail the same way; the outcome, a full run, is the same |

## Design

- Kinetra `ci.yml` and `docs/testing.md`, Lab `ci.yml`: the header says the docs check runs only on `arch`, except that the `changes` job runs it when it skips the build; the table's *Runs on* says the same. Lab's `README.md` adds that `checks` also passes when the `changes` job skipped the build and ran the docs check itself.
- Lab `ci.yml`, `changes` job: the *No decision script* warning reads "Could not read tools/release/needs_build.sh at the base branch's tip $BASE: the file is missing there, or fetching it failed. The full CI runs."
- `capture_tour.sh`: `mkdir -p "$(dirname "$output")"` moves from after the render to before it, and the work folder is made next to the output (`mktemp -d "$(dirname "$output")/.tour.XXXXXX"`) right after it, with a comment on why. A folder that cannot be made or written fails there, and the final `mv` is a rename on one file system.

## Affected areas

Kinetra: `.github/workflows/ci.yml` (comment), `docs/testing.md`. Lab: `.github/workflows/ci.yml` (comment and warning text), `README.md`, `tools/capture/capture_tour.sh`, this document, the design index and a review lesson.

## Test plan

- `capture_tour.sh` with a fake `kinetra_capture` and `ffmpeg`, an output path below a regular file so `mkdir` fails: before the change, the tool rendered (`--tour -`) and then the script failed; after it, the script exits 1 before calling the tool.
- The same with an output folder that exists but cannot be written (`/proc`, which even root cannot write): before, the tool rendered and the `mv` failed; after, `mktemp` fails before the tool is called.
- The same with a writable new nested folder: the tool renders, the video lands there, and no `.tour.*` folder is left.
- Both workflows parse as YAML; `bash -n` on the script.
- Full level: the pull requests' CI. `kinetra_capture_tests` does not run `capture_tour.sh`, which needs ffmpeg and renders for minutes.

## Review

- The author's review of both diffs; the copies check for "docs check" with "only" in both repositories found Lab's `ci.yml` header, now changed too.
- Agent review, round 1 (on 6f3e5b5, *Fix first*):
  - Blocking: `README.md:148` named the command `check_docs.py`, not "docs check", so the search missed it; it now names the `changes` job. Kinetra's `ci.yml` header, which the reviewer also named, already says that `checks` passes when `changes` skipped the build (lines 24–26), so Kinetra needs no change. The reviewer's lesson on searching a step's command and name went to Lab's page.
  - Should fix: an output folder that exists but cannot be written still lost the render at the `mv`; the work folder now sits next to the output and fails before the render.
  - Nit: this document gained *Open points*.

## Verification

- `capture_tour.sh` runs of the test plan: below a regular file and in `/proc`, the old version renders then fails, the new one exits 1 with no tool call; the writable case writes the video and leaves no work folder.
- `python3 -c "import yaml; …"` on both `ci.yml` files; `bash -n tools/capture/capture_tour.sh`.
- `python3 external/Kinetra/tools/check_docs.py` and Kinetra's `tools/check_docs.py`: all links resolve.
- Not run locally: the build of `kinetra_capture_tests` and `kinetra_capture`, which need the runtime, and this session's container has no SDL2. Neither builds nor reads `capture_tour.sh`; CI builds and runs them.
- CI of Kinetra#39 and of this pull request.

## Pull request

**fix: carry out the code nits of Kinetra#37 and #83's last review rounds**

- **Summary:** three nits that touch CI or a script, left after the last review rounds of Kinetra#37 and #83: where the docs check runs, the *No decision script* warning, and an output folder that `capture_tour.sh` cannot make or write, which now stops it before the render.
- **Changes:** Lab's `ci.yml` header and warning text; `README.md`'s CI paragraph; `capture_tour.sh` (the output folder and the work folder next to it before the render); this document; one review lesson. Kinetra#39 changes Kinetra's header and testing table.
- **Design:** this document.
- **Verification:** the `capture_tour.sh` runs with fakes (a folder that cannot be made, one that cannot be written, a writable one), YAML, `bash -n`, the docs check; CI.
- **Review notes:** no CI decision changes, only a comment and a warning text. Agent review round 1 is answered in the commits named under *Review*.
- **Dependencies and merge order:** Kinetra#39 is merged; this pull request does not depend on it, since Lab does not use Kinetra's comment. It changes CI, so the user merges it. A pointer-only pull request then records Kinetra `main`.
- **Follow-up:** none.

## Open points

None.
