# Roadmap as a backlog

Status: implemented · Branch: `ccr-16c06656-sxr8ek` · Related: [roadmap](../roadmap.md), [Kinetra design](../../external/Kinetra/docs/design/2026-11-21-roadmap-backlog.md), [quality roadmap](2026-11-18-quality-roadmap.md)

Lab gets a standing backlog in `docs/roadmap.md`, in the format Kinetra defines, with an inventory of what the quality roadmap and the open points leave and the user's long-term ideas. The `roadmap` skill and the rules come from Kinetra, so this branch also moves the submodule pointer.

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

The user asked whether the quality roadmap is finished, and for one place where planned work, brainstorm ideas and to-do lists end up.

Inventory on 2026-11-21, first at Lab `main` `c77f11a`, refreshed against `0bc3eb4` plus #62 (`docs/dotio-module`), which merges before this branch:

| Phase | State |
| --- | --- |
| 1–4 | Merged |
| 5 Demo | Merged (#52, #54, #55), except step 4: the contact tests to Kinetra were committed in a session without Kinetra push access and never reached GitHub |
| 6 Dotio | Merged (#50, #51, #53, #56, #57, #62, Kinetra #17); the user's playtest is open |
| 7 Predictive passing | Merged (#49, #58, #59, #60); the MAKE WAY policy waits for the user's decision |
| 8 Shared passing | Not started; can start now |
| 9 Easier try-out | Merged (#47, #48); the desktop check on Windows is open |
| Documentation cleanup | Done except the skill READMEs (Kinetra) |

So the quality roadmap is not finished: phase 8 and a few decisions and checks by the user remain. Further open work sits in the [WSLg frame pacing](2026-11-20-wslg-frame-pacing.md) draft and in the open points of the module pages.

## Goals and non-goals

- Goal: `docs/roadmap.md` with every remaining item, so the user can start from it.
- Goal: the user's ideas recorded: Kinetra-Lab on Android, and a 3D lab based on the Skatepark repository.
- Non-goal: changing the quality roadmap. Other sessions were updating its status table for phases 5 to 7.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| One roadmap or one per repository | One per repository (user's choice) | See the [Kinetra design](../../external/Kinetra/docs/design/2026-11-21-roadmap-backlog.md#decisions) |
| Start now or after phase 8 | Now, quality roadmap untouched (user's choice) | The running phases are items under *Now* that link their scope; no conflicts with the other sessions |

## Design

The roadmap follows the [format](../../external/Kinetra/docs/documentation.md#roadmap). Running phases are under *Now*, items that wait for a decision by the user under *Next*, blocked follow-ups under *Later*, and the Android and Skatepark ideas under *Ideas*, each linked to the Kinetra idea it depends on.

## Affected areas

`docs/roadmap.md` (new), `AGENTS.md`, `docs/README.md`, the design index, this document, two stale lines found while checking (`tools/capture/README.md` still called Dotio's captures irreproducible; `launcher/README.md` named only `kinetra_i4sim` as taking a seed), and the `external/Kinetra` pointer (Kinetra `docs/roadmap-backlog`, SMB-M87/Kinetra#18). The pointer on `main` names `3622849`, the branch commit of Kinetra #17 rather than its merge commit; the new pointer descends from both.

## Test plan

Documentation and a pointer to a documentation-only Kinetra commit: the Docs level, in Lab and in Kinetra.

## Review

Checked that every item links its source, and that the phase items repeat no scope from the quality roadmap.

## Verification

- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.
- In Kinetra: `python3 tools/check_docs.py`: all links resolve.

## Pull request

**docs(roadmap): add a backlog with the remaining work and ideas**

- **Summary:** `docs/roadmap.md` lists the work left after the quality roadmap and the user's ideas; agents keep it current with Kinetra's new `roadmap` skill.
- **Changes:** the roadmap; rows in AGENTS.md and the documentation index; two stale lines in the capture and launcher READMEs; the Kinetra pointer.
- **Design:** this document and the Kinetra design.
- **Verification:** Docs level in both repositories.
- **Dependencies and merge order:** after #62. Kinetra SMB-M87/Kinetra#18 merges first, with a merge commit; then move the pointer to that merge commit.
- **Follow-up:** the items in the roadmap.

## Open points

- After the Kinetra merge, the pointer moves to Kinetra's merge commit before this pull request merges.
