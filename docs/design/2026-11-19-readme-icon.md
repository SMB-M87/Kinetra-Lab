# README icon

Status: implemented · Branch: `docs/readme-icon` · Related: [Kinetra design](../../external/Kinetra/docs/design/2026-11-19-readme-icon.md), [window icon](2026-11-19-window-icon.md)

The user asked for the Kinetra icon at the top of the README in Kinetra and in Lab. This document covers the Lab half; the decisions on placement and size are in the [Kinetra design](../../external/Kinetra/docs/design/2026-11-19-readme-icon.md#decisions).

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

- [README.md](../../README.md) starts with plain text. Lab's applications use the engine icon (`external/Kinetra/resources/kinetra-icon.png`), but the project page does not show it.

## Goals and non-goals

- Goal: the Lab README opens with the same icon as the Kinetra README, with the same layout.
- Non-goal: a Lab-specific icon. Lab's applications share the engine icon (see the [window icon](2026-11-19-window-icon.md) design).

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Placement and size | As in Kinetra: centered above the title, 128 × 128 | The two project pages look alike. |
| Source | The raw URL of the icon on Kinetra `main` | GitHub does not serve files from inside a submodule, so a relative path to `external/Kinetra/resources/` renders as a broken image there. A copy in Lab would be a second source that can drift. Kinetra is public, so the raw URL renders for every reader. |

## Design

A `<p align="center">` block with one `<img>` sits above `# Kinetra-Lab`, pointing at `https://raw.githubusercontent.com/SMB-M87/Kinetra/main/resources/kinetra-icon.png`. The URL follows Kinetra `main`, so a future icon change shows up in Lab without a Lab commit. Offline Markdown viewers show the `alt` text instead.

## Affected areas

- [README.md](../../README.md)
- This document and the [design index](README.md#index)
- No submodule pointer change: the icon is already on Kinetra `main`.

## Test plan

Docs level: `python3 external/Kinetra/tools/check_docs.py`. The checker does not fetch external URLs, so the image URL is checked by hand.

## Review

Reviewed the diff by hand: only the README header and the design documents changed, the `alt` text is present, and the URL points at Kinetra `main`, not at a branch. No findings.

## Verification

- `python3 external/Kinetra/tools/check_docs.py`: passed.
- `curl -sIL https://raw.githubusercontent.com/SMB-M87/Kinetra/main/resources/kinetra-icon.png`: `HTTP/2 200`, `content-type: image/png`.

## Pull request

**Title:** `docs(readme): show the Kinetra icon at the top`

**Summary:** the Lab README opens with the Kinetra icon, centered above the title, like the Kinetra README.

**Changes:** a centered `<img>` (128 × 128) above `# Kinetra-Lab`, loaded from the icon on Kinetra `main`; this design document.

**Design:** [docs/design/2026-11-19-readme-icon.md](https://github.com/SMB-M87/Kinetra-Lab/blob/docs/readme-icon/docs/design/2026-11-19-readme-icon.md)

**Verification:** `python3 external/Kinetra/tools/check_docs.py` passed; the image URL returns `200 image/png`. Docs level, no build.

**Review notes:** documentation only. A relative path into the submodule does not render on GitHub, hence the raw URL.

**Dependencies and merge order:** independent of the Kinetra pull request; no pointer change.

**Follow-up:** none.

## Open points

- None.
