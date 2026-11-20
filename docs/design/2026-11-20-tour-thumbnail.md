# Tour thumbnail in the README

Status: accepted · Branch: `feat/tour-thumbnail` · Related: [generated tour video](2026-11-19-tour-video.md), [media guide](../media/README.md), [capture tool](../../tools/capture/README.md)

The README links the tour video with one line of text below the gallery, which is easy to miss. The user asked whether the video could play in the README itself. On a free GitHub plan it cannot, so the README now opens its gallery with a clickable thumbnail of the tour. The thumbnail is generated from the video, like every other image in `docs/media`.

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

- The only way into the tour is `[Watch the full tour](https://youtu.be/jkPwx831MOU).` below the launcher screenshots ([README.md:80](../../README.md)). A reader who skims the gallery does not see that a five-minute video exists.
- GitHub shows a video player in a README only for a video uploaded as an attachment (`https://github.com/user-attachments/assets/...` on a line of its own). The limit is 10 MB on a free plan and 100 MB on a paid plan ([GitHub Docs: attaching files](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/attaching-files)). The tour is 93 MB ([media guide](../media/README.md#files)), and the repository is on a free plan. GitHub also removes `<iframe>`, so the YouTube player cannot be embedded either.

## Goals and non-goals

- Goal: the gallery opens with a picture of the tour that links to the YouTube copy and looks like a video, with a play button.
- Goal: the picture is generated from the video by a script, so a new render of the tour brings a matching thumbnail.
- Non-goal: a short teaser video under 10 MB. That would need an upload in the browser for every new render, and the user chose the thumbnail.
- Non-goal: changes to the tour itself or to `kinetra_capture`.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Video in the README or a thumbnail | A thumbnail that links to YouTube | Settled with the user: the free plan's 10 MB limit rules out the 93 MB tour, and a teaser under 10 MB would lose too much of it. |
| What the thumbnail shows | A 2 × 2 mosaic: the launcher, Demo, Dotio and i4sim | Settled with the user out of three rendered candidates (the mosaic, one Demo frame, one i4sim frame). The tour shows the launcher and all three applications, and the mosaic says that at a glance. |
| Where it goes | At the top of *Screenshots and demo*, instead of the text link at the bottom | Settled with the user: the first thing in the gallery, while the README still opens with its introduction and table of contents. |
| Source of the picture | Generated from `kinetra-lab-tour.mp4` with ffmpeg, committed as a PNG | Every image in `docs/media` comes from a script. YouTube's own thumbnail URL would be an outside image that the repository does not control. |
| Script | A separate `tools/capture/tour_thumbnail.sh`, run by `capture_tour.sh` after it encodes the video | The thumbnail can be refreshed from the existing video in seconds, without rendering the tour again for minutes. |
| Which frames | Four fixed times in the tour, named in the script | The tour's timing comes from its script and only changes when the script does; its maintainers already look at frames of every chapter after a change. A caption-to-frame index from `kinetra_capture` would be more robust, but is more code than four timestamps need. |
| Play button | A dark translucent disc with a white triangle, drawn by ffmpeg's `geq` filter | No font and no image file are needed, and the tour's video stays untouched. |
| Size and format | PNG at 1280 × 720, about 0.6 MB | Sharp on high-density screens at GitHub's README width; PNG like the other screenshots. |

## Design

`tour_thumbnail.sh [video] [output-file]` reads the video's length with ffprobe and stops with a message when a moment lies past its end. It then runs one ffmpeg command:

```mermaid
flowchart LR
    Video["kinetra-lab-tour.mp4"] -->|"-ss 4"| L["Launcher"]
    Video -->|"-ss 94"| D["Demo: CLICK SETS GOAL"]
    Video -->|"-ss 158"| O["Dotio: ZOOMED OUT"]
    Video -->|"-ss 276"| I["i4sim: LAYERS"]
    L & D & O & I -->|"scale 640 × 360"| M["xstack 2 × 2"]
    P["geq: disc and triangle"] --> V["overlay in the middle"]
    M --> V --> Out["kinetra-lab-tour-thumbnail.png"]
```

*Takeaway:* four frames of the existing video become one picture; nothing is rendered again.

- The output defaults to the video's name with `-thumbnail.png`, so `capture_tour.sh` with a custom output file puts the thumbnail next to it.
- ffmpeg writes to a temporary file, which replaces the output only when it is not empty. A failed run therefore never leaves a stale or broken thumbnail.
- The README shows the picture as a Markdown image inside a link to `https://youtu.be/jkPwx831MOU`, with a sentence below it that names the length and contents, for readers who do not see images.

## Affected areas

- New: [tools/capture/tour_thumbnail.sh](../../tools/capture/tour_thumbnail.sh) and [docs/media/kinetra-lab-tour-thumbnail.png](../media/kinetra-lab-tour-thumbnail.png).
- [tools/capture/capture_tour.sh](../../tools/capture/capture_tour.sh): runs the thumbnail script after encoding.
- [README.md](../../README.md#screenshots-and-demo): the thumbnail at the top of the gallery; the text link at the bottom goes.
- [docs/media/README.md](../media/README.md) and [tools/capture/README.md](../../tools/capture/README.md): the new file and script, and why the README shows a thumbnail.
- This document and the [design index](README.md#index).
- No C++ code, CMake or public API changes.

## Test plan

| Behavior | How it is checked | Level |
| --- | --- | --- |
| The script makes the mosaic with the play button from the committed video | Run `tools/capture/tour_thumbnail.sh` and look at the PNG | By hand |
| A moment past the end of the video stops the script, and an existing thumbnail stays as it was | Run it on a short cut of the video and check the exit status and the file | By hand |
| `capture_tour.sh` calls the script with its output file | Read the diff, then `bash -n` and shellcheck on both scripts. A full tour render takes minutes and would replace the committed video with a new Dotio take, so it is not run | By hand |
| The README and media guide still name every captured file, which `kinetra_capture_tests` reads | `cmake --build --preset dev --target kinetra_capture_tests kinetra_capture` and `ctest --preset dev -R '^kinetra_capture'` | Targeted (tools/capture) |
| Links and paths in the changed Markdown | `python3 external/Kinetra/tools/check_docs.py` | Docs |

The graphics smoke test (`kinetra_capture_smoke`) runs `kinetra_capture` itself and not the scripts, so this change does not touch it.

## Review

To be filled after the review.

## Verification

To be filled after verification.

## Pull request

To be filled before the pull request.

## Open points

- None yet.
