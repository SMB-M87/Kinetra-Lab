# Tour version note

Status: implemented · Branch: `docs/tour-version-note` · Related: [tour thumbnail in the README](2026-11-20-tour-thumbnail.md), [media guide](../media/README.md)

Below the tour thumbnail, the README repeated the YouTube link in a sentence. The user asked to replace that sentence with a note on versions: the YouTube copy can be older than the code, and the newest render is the file in the repository, which is itself only regenerated after larger changes.

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

- [README.md](../../README.md#the-labs) shows the thumbnail as a link to YouTube, then links the same video again in the next sentence ("The full tour on YouTube (5 min 12 s): ...").
- Nothing tells a reader that the YouTube copy is uploaded by hand and can show an older state than the code. The newer render in `docs/media/kinetra-lab-tour.mp4` is not mentioned in the README at all.

## Goals and non-goals

- Goal: the sentence below the thumbnail says that the YouTube copy can be older, points at `kinetra-lab-tour.mp4` for the latest render, and says that this file is regenerated after larger changes, not after every commit.
- Goal: the media guide says the same, so maintainers know when to render and upload.
- Non-goal: changes to the thumbnail, the scripts or the video.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| The sentence below the thumbnail | A version note instead of a second link to YouTube | Asked by the user: the thumbnail is already the link, and readers need to know which copy is newest. |
| How often the tour is rendered | After larger visible changes, not after every commit | The user's practice. A render takes minutes and adds 93 MB to the history each time. |

## Design

The README sentence reads: "The YouTube copy can be older than the code. The latest render is kinetra-lab-tour.mp4 in this repository; it is regenerated after larger changes, so it can trail the newest commits as well." The link goes to the file in `docs/media`. GitHub offers it for download because it is too large to show in the browser.

In the [media guide](../media/README.md), the tour's row says that the README points at the file for the latest render. The refresh section says when to render the tour, and that a new YouTube upload needs a new link in the README and the guide.

## Affected areas

- [README.md](../../README.md#the-labs): the sentence below the thumbnail.
- [docs/media/README.md](../media/README.md): the tour's row and the refresh section.
- This document and the [design index](README.md#index).

## Test plan

Docs level: `python3 external/Kinetra/tools/check_docs.py`. `kinetra_capture_tests` reads the README for the clip file names, which this change does not touch.

## Review

Reviewed the diff by hand: one sentence in the README and two in the media guide. All three say the same about which copy is newest. No findings.

## Verification

- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.

## Pull request

**Title:** `docs(readme): say which copy of the tour is newest`

**Summary:** the sentence below the tour thumbnail no longer repeats the YouTube link. It now says that the YouTube copy can be older than the code, and that the latest render is `docs/media/kinetra-lab-tour.mp4`, which is regenerated after larger changes.

**Changes:** the README sentence; the tour's row and the refresh section in the media guide; this design document.

**Design:** [docs/design/2026-11-20-tour-version-note.md](https://github.com/SMB-M87/Kinetra-Lab/blob/docs/tour-version-note/docs/design/2026-11-20-tour-version-note.md)

**Verification:** `python3 external/Kinetra/tools/check_docs.py` passed. Docs level, no build.

**Review notes:** documentation only. Follows up on SMB-M87/Kinetra-Lab#35.

**Dependencies and merge order:** no Kinetra change and no pointer change. Open PR SMB-M87/Kinetra-Lab#34 edits other lines of `README.md`.

**Follow-up:** none.

## Open points

- None.
