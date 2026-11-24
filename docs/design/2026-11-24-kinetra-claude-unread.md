# Kinetra's .claude/ does not build Lab

Status: implemented · Branch: `ccr-f93b2da2-0j3r2i`, in Kinetra `docs/claude-unread-by-lab` · Related: Kinetra-Lab#119, Kinetra-Lab#121, [Kinetra's review access](../../external/Kinetra/docs/design/2026-11-24-review-access-and-agent-merges.md)

A pointer move whose Kinetra range changes `.claude/settings.json` ran Lab's full CI, although Lab's build does not read that file. `tools/release/needs_build.sh` now counts Kinetra's `.claude/` with the other Kinetra paths Lab's build does not read, so such a pointer pull request runs only the docs check.

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

- Kinetra-Lab#119 (Kinetra `312ccf7..8484674`) and Kinetra-Lab#121 (`8484674..8e17d51`) ran the full CI, about seven minutes each. Their Kinetra ranges hold only Markdown, `docs/`, `.agents/` and `.claude/settings.json`, from Kinetra#56 and Kinetra#57.
- The `changes` job of #121 logged *build: Kinetra files outside Markdown, docs/, .agents/, .github/ and tools/ changed*: `kinetra_unread` in [needs_build.sh](../../tools/release/needs_build.sh) lacks `.claude/`.
- Kinetra's `.claude/` holds `settings.json` (a Claude Code session's permission rules and SessionStart hook) and the `skills` link to `.agents/skills`. No Lab build step, test or zip reads it; Lab's own CI does not check out Kinetra's hook.

## Goals and non-goals

Goals:

1. A pointer move whose Kinetra range adds only `.claude/` to the paths that skip today runs only the docs check, in CI and in the release workflow.
2. A range that also changes engine sources, or Kinetra's `README.md` or `docs/media/`, still builds.
3. Every page that lists the Kinetra paths Lab's build does not read names `.claude/`.

Non-goals:

- Lab's own `.claude/settings.json` still builds and is not docs-only: its permission rules are reviewed in Lab, as any non-documentation file.
- Kinetra's own CI, which builds when Kinetra's `.claude/` changes; that run takes about two minutes.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Where | `kinetra_unread` in `needs_build.sh`, next to `.github/` and `tools/` | The one decision both CI and the release workflow use; Kinetra paths only, as for `.github/` and `tools/` |
| Lab's own `.claude/` | Unchanged: it builds | Changing it would also make it docs-only under Lab's rule, and a change to permission rules should keep its review |

## Design

`kinetra_unread` becomes `(\.md$|^docs/|^\.agents/|^\.claude/|^\.github/|^tools/|^$)`. The script's header, its messages, the comments of `ci.yml` and `release.yml`, the release tool's README, Lab's `AGENTS.md` and `external/README.md`, and in Kinetra `AGENTS.md`, the workflow and the `verify` skill name `.claude/` in the list. The `changes` job runs the base branch's copy, so the filter applies to pointer pull requests opened after this merges; this pull request's own copy decides to build it, because it changes the script.

## Affected areas

| Repository | Files |
| --- | --- |
| Kinetra-Lab | `tools/release/needs_build.sh`, `tools/release/README.md`, `.github/workflows/ci.yml` and `release.yml` (comments), `AGENTS.md`, `external/README.md`, this document and the index |
| Kinetra | `AGENTS.md` (docs-only paragraph), `docs/workflow.md` (changes across Kinetra and Lab), `.agents/skills/verify/SKILL.md` |

## Test plan

| Behavior | Proof | Level |
| --- | --- | --- |
| #121's range skips with the new filter and built with the old one | Both scripts on Lab `81c96a6..30036c3`, the merge base and head of #121 | Targeted |
| A range with engine sources still builds | A local pointer commit `bc3dde3 → e5b99d8` (Kinetra's `math.h` and tests) | Targeted |
| A range with Kinetra's `README.md` still builds | A local pointer commit `8484674 → b86a2ed` | Targeted |
| Lab's own `.claude/` still builds | A local commit that changes only Lab's `.claude/settings.json` | Targeted |
| CI on this pull request | The script changed, so its own copy decides to build; the full CI runs | Full (CI) |

## Review

Filled in after the agent review.

## Verification

- `bash -n tools/release/needs_build.sh`: no syntax error.
- On #121's range (`81c96a6..30036c3`): the old script printed `true` (*Kinetra files outside Markdown, docs/, .agents/, .github/ and tools/ changed*), the new one `false`.
- Local throwaway commits on `4947a52`, run with the new script: pointer `bc3dde3 → e5b99d8` printed `true` (engine sources); `8484674 → b86a2ed` printed `true` (Kinetra's `README.md` or `docs/media/`); only Lab's `.claude/settings.json` printed `true` (Lab files outside Markdown, `docs/` and `.agents/`); `8484674 → 8e17d51` printed `false`.
- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.
- Not run: the local cross build and `package_windows.sh` that Lab's table names for `tools/release/`; this change touches only the decision whether to build, not the build or the zip, and CI runs the decision on this pull request.

## Pull request

**Title:** `fix(ci): a Kinetra range with only .claude/ does not build Lab`

See the pull request description.

## Open points

None.
