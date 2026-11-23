# Copies check in Lab

Status: implemented · Branch: `ccr-915142f6-86mj8n` · Related: Kinetra design *Copies check* (Kinetra #36), Lab #81

Kinetra #36 added `tools/check_copies.py` and made the `verify` skill run it at every level; Lab #81 recorded that Kinetra commit. Lab's own verification section in [AGENTS.md](../../AGENTS.md#verification-in-lab) lists the Lab commands per level, so it names the script too, with Lab's path.

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

A missed copy of a changed name came back four times in Kinetra and twice in Kinetra-Lab #78. Kinetra's rules now point to the script, but Lab's verification table names only the docs check.

## Goals and non-goals

Goal: Lab's verification section names `external/Kinetra/tools/check_copies.py` for every level. Non-goal: changing the script, which lives in Kinetra.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Where in Lab | One sentence above the verification table, not a table row | The script runs at every level, as the `verify` skill says |

## Design

A sentence above the table in *Verification in Lab*: the command with Lab's path, `--term` for reworded rules, what it lists, and a link to Kinetra's tools page.

## Affected areas

`AGENTS.md`, this document, the [index](README.md#index).

## Test plan

| Behavior | Proof | Level |
| --- | --- | --- |
| The command works in Lab | `python3 external/Kinetra/tools/check_copies.py` on this branch | Docs |
| Links resolve | `python3 external/Kinetra/tools/check_docs.py` | Docs |

## Review

Filled in after review.

## Verification

On this branch, `python3 external/Kinetra/tools/check_copies.py` reports no hit and `python3 external/Kinetra/tools/check_docs.py` passes.

## Pull request

Title: `docs(agents): run check_copies.py in Lab's verification`

**Summary.** Lab's verification section names Kinetra's new `tools/check_copies.py` (Kinetra #36, recorded by Lab #81), which lists the names a change removes that a page still names; the `verify` skill already runs it at every level.

**Changes.** `AGENTS.md`: one sentence above the verification table. This design document and its index row.

**Design.** `docs/design/2026-11-23-copies-check-in-lab.md`.

**Verification.** Both scripts pass on this branch; Docs level.

**Review notes.** Changes the agent rules, so it waits for the user.

**Dependencies and merge order.** After Lab #81, which is merged.

**Follow-up.** None.

## Open points

None.
