# Link the architecture decisions from the Lab docs

Status: accepted · Branch: `docs/lab-adr-index` · Related: [Kinetra architecture decisions](../../external/Kinetra/docs/decisions/README.md), [ADR rule in Lab](../../AGENTS.md#documentation-in-lab)

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

- [docs/README.md](../README.md) has no link to any ADR. Only [labs/demo/README.md](../../labs/demo/README.md#verification-and-limits) links three of them, inline.
- ADR 0001 to 0007 record Demo and Dotio decisions from before the applications moved to Lab, and ADR 0009 defines the Kinetra/Lab boundary. They stay in Kinetra because ADRs are history and are not moved.
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
- a link to the Kinetra ADR index, naming 0001 to 0007 as the Demo and Dotio history and linking 0009 as the boundary;
- one sentence on where Lab-only ADRs go, linking the rule in `AGENTS.md`.

## Affected areas

- `docs/README.md`: one table row.
- `docs/design/`: this document and its index row.

## Test plan

Documentation only, so the **Docs** level applies: `python3 external/Kinetra/tools/check_docs.py` passes, so every new link and anchor resolves.

## Review

Filled in after the review.

## Verification

Filled in after verification.

## Pull request

Filled in before the pull request.

## Open points

- The first Lab ADR creates `docs/decisions/README.md` and then gets a row in `docs/README.md` next to (or instead of) this one.
