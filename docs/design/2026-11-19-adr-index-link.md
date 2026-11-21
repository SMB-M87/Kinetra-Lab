# Link the architecture decisions from the Lab docs

Status: implemented · Branch: `docs/lab-adr-index` · Related: [Kinetra architecture decisions](../../external/Kinetra/docs/decisions/README.md), [ADR rule in Lab](../../AGENTS.md#documentation-in-lab)

The Lab documentation index gets a link to Kinetra's architecture decisions. Readers who start in Lab can then find the decisions behind Demo, Dotio and the Kinetra/Lab boundary, and see why Lab has no `docs/decisions/` of its own yet.

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

- [docs/README.md](../README.md) has no link to any ADR. Only [labs/demo/README.md](../../labs/demo/README.md#open-points) links three of them, inline.
- ADR 0002 to 0007 record Demo and Dotio decisions from before the applications moved to Lab, and ADR 0009 defines the Kinetra/Lab boundary. They stay in Kinetra because ADRs are history and are not moved.
- Lab has no `docs/decisions/` directory. [AGENTS.md](../../AGENTS.md#documentation-in-lab) creates it with the first Lab ADR, and every decision so far belonged in Kinetra. A reader of the Lab docs sees neither the Kinetra ADRs nor this rule, so the missing directory looks like an omission.

## Goals and non-goals

- Goal: from the Lab documentation index, a reader reaches the Kinetra ADR index and learns where Lab-only decisions will go.
- Non-goal: creating `docs/decisions/` or writing a Lab ADR. The first candidate is the open layer point about `kinetra_i4sim_presentation` and `Kinetra::Launcher`, which [roadmap phase 4](2026-11-18-quality-roadmap.md#open-points) settles.
- Non-goal: changes in Kinetra.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Do it | Add the link | Asked by the user on 2026-11-19 |
| Link target | `external/Kinetra/docs/decisions/README.md`, not `docs/README.md#decisions` in Kinetra | The file exists at the Kinetra commit on Lab `main` and at the newer one on the open i4sim branch. Kinetra's documentation cleanup moved the numbered index into it, so the link stays correct when the pointer moves |
| Place | A row in the *Architecture guides* table of `docs/README.md` | That table answers "where do I read about X"; one row is enough and needs no new section |

No decision meets the ADR criteria.

## Design

One new row in the *Architecture guides* table, between *Launcher and overlays* and *Design documents*:

- the question it answers: why the boundaries are the way they are;
- a link to the Kinetra ADR index, naming 0002 to 0007 as the Demo and Dotio history and linking 0009 as the boundary;
- one sentence on where Lab-only ADRs go, linking the rule in `AGENTS.md`.

## Affected areas

- `docs/README.md`: one table row.
- `docs/design/`: this document and its index row.

## Test plan

Documentation only, so the **Docs** level applies: `python3 external/Kinetra/tools/check_docs.py` passes, so every new link and anchor resolves.

## Review

A read of `git diff main...HEAD` against the review list in the [workflow](../../external/Kinetra/docs/workflow.md#6-review); for a one-row documentation diff this replaces `/code-review`.

| Finding | Resolution |
| --- | --- |
| The row named ADR 0001 to 0007 as Demo and Dotio decisions, but 0001 records the engine foundation | Changed to 0002 to 0007 in the row and in this document |
| The link targets must survive the pointer move on the open i4sim branch | Checked: `docs/decisions/README.md` and `0009-core-and-lab.md` exist at both Kinetra commits; at the newer one the README holds the full numbered index |
| "Lab has no ADRs yet" goes stale with the first Lab ADR | Kept: the `adr` skill names `docs/README.md` as the Lab ADR index, so that change updates this row. Listed under open points |

## Verification

Level **Docs**; no build, because only Markdown changed.

- `python3 external/Kinetra/tools/check_docs.py`: "All links resolve in 37 Markdown files", with the submodule at the Kinetra commit recorded on `main`.
- `git -C external/Kinetra cat-file -e d6ffdc6:<path>` for both link targets: present at the Kinetra commit of the open i4sim branch.

## Pull request

Title: `docs: link the architecture decisions from the documentation index`

```markdown
## Summary

The Lab documentation index had no link to any ADR, so the decisions behind Demo, Dotio and the Kinetra/Lab boundary were hard to find, and the missing `docs/decisions/` looked like an omission. One new row in `docs/README.md` links Kinetra's ADR index and states where Lab-only ADRs will go.

## Changes

- `docs/README.md`: a row in *Architecture guides* that links the Kinetra ADR index, names 0002 to 0007 as the Demo and Dotio history, links 0009 as the boundary, and points to the Lab ADR rule in `AGENTS.md`.
- `docs/design/`: the design document and its index row.

## Design

[docs/design/2026-11-19-adr-index-link.md](docs/design/2026-11-19-adr-index-link.md). No ADR.

## Verification

Docs level: `python3 external/Kinetra/tools/check_docs.py` passes (all links resolve in 37 Markdown files). Both link targets also exist at the newer Kinetra commit on the open i4sim branch. No build: only Markdown changed.

## Review notes

The link goes to `external/Kinetra/docs/decisions/README.md` rather than an anchor in Kinetra's `docs/README.md`, because Kinetra's documentation cleanup moved the numbered index there.

## Dependencies and merge order

None. Independent of the open i4sim pull request; both add a row at the top of the design index, so the second to merge may need a one-line conflict resolution there.

## Follow-up

The first Lab ADR (likely the i4sim presentation's `Kinetra::Launcher` edge, roadmap phase 4) creates `docs/decisions/README.md` and updates this row.
```

## Open points

- The first Lab ADR creates `docs/decisions/README.md` and then gets a row in `docs/README.md` next to (or instead of) this one.
