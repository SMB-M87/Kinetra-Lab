# Roadmap: GIFs, a Demo scenario menu, a launcher preview, an evolution lab and a leaner cycle

Status: implemented · Branch: `claude/roadmap-overlays-gifs-agents-jdz3yc` · Related: [roadmap](../roadmap.md), [Kinetra design](https://github.com/SMB-M87/Kinetra/blob/main/docs/design/2026-11-23-roadmap-media-labs-cycle.md), [media guide](../media/README.md)

On 2026-11-23 the user asked for seven additions to the roadmaps: GIFs in both READMEs with a rule to keep them current, the end of the routine tour render, a preview of each lab in the launcher, an evolution lab, an answer on splitting the repository, a leaner agentic cycle, and a scenario menu for Demo. This branch adds them as items in Lab's and Kinetra's roadmaps; it changes no code, media or rules.

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

The user's wishes, translated:

1. While the pointer is on a lab in the launcher, an overlay shows a picture or GIF of that lab with a description, so new users get an image of it.
2. Kinetra's README shows the shared UI in GIFs, captured from Demo: the launcher, the lab menu for a blueprint or a difficulty, the settings panel, the debug menu, the agent inspector, the frame times and the other debug views.
3. Lab's README is about the labs themselves, with as many GIFs as possible, because a GIF shows more than a static picture. Agents refresh the GIFs whenever they change something visible, in Lab and in Kinetra. The long tour MP4 is then no longer generated; the user wants to get rid of it.
4. Once the current roadmap items are done, the user makes one clear and extensive video, posts it on YouTube and puts the new link in the README; then the MP4 file leaves the repository.
5. A new lab with evolutionary algorithms: autonomous agents that perhaps communicate, learn from their behavior and evolve, without language models. The user asks how the agent would approach it and how far it could go.
6. Whether the repository should split into a repository per lab, runnable alone or inside Lab; the user thinks the current layout is right but has doubts. And the agentic cycle should be optimized.
7. In answer to the hand-over: the agentic cycle is picked up first; and before Kinetra's GIFs of the shared UI, Demo gets a menu in which a choice starts a Demo environment that showcases one part of the engine.

Facts checked on 2026-11-23: `LaunchChoice::description` exists, but `LaunchMenu` does not draw it, and the runtime draws only colored shapes and bitmap text, no images. Demo has no configuration page. Kinetra's README has no media. A clone holds three renders of the tour video (61, 89 and 94 MB). From 18 to 22 November Lab merged 77 pull requests, 11 of which only moved the Kinetra pointer, and 12 topics have a design document in both repositories.

## Goals and non-goals

- Goal: every wish is an item in the right roadmap, with the facts it rests on and its open questions; work that needs both repositories has an item in each.
- Goal: the existing item that would render the tour again no longer does.
- Non-goal: changing the agent rules now. The GIF rule lands with its item, when Kinetra's GIFs exist; Lab's [AGENTS.md](../../AGENTS.md#documentation-in-lab) already asks for refreshed media after a visible change.
- Non-goal: removing the tour video or its tool now; the user removes the file after the new upload.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Sections | GIFs, the Demo scenario menu and the leaner cycle in *Next*; the launcher preview and the YouTube video at the end of *Later*; the evolution lab and the repository question under *Ideas* | The user asked for the first two and each has a concrete first step; the preview waits for images in Kinetra; the video waits for everything else; the last two are an idea and a question, not agreed work |
| Position | *Leaner agentic cycle* first among the agent's work, in both roadmaps; *Demo scenario menu* right before *README media as GIFs*; the other new items at the end of their sections | The user's answers to the hand-over; the user's checks stay first in *Next*, since the agent does not pick them up |
| Demo scenario menu and the GIFs | Kinetra's GIFs depend on the menu; the Lab README's GIFs do not | The user wants the configuration page shown with Demo, which has none yet |
| Tour video | `capture_tour.sh` renders it, outside `docs/media`, so the tool stays; it waits for the items of *Next* and *Later*, not the *Ideas* | The user's answers to the hand-over |
| Both READMEs | One Lab item, *README media as GIFs*, with Kinetra's *Shared UI in the README* | One capture tool, one rule and one design serve both READMEs |
| Repository split | Recorded as an idea with the agent's advice to keep one Lab repository | The user doubted it; the advice and its reasons stay findable, as for *Documentation as a site* |
| Kinetra pointer | Not moved | Lab names Kinetra's new items and links their sections; a pointer-only pull request follows the Kinetra merge |

## Design

Lab's [roadmap](../roadmap.md):

- *Next*: **Leaner agentic cycle** (wish 6, with the measured facts and a list of candidate measures), **Demo scenario menu** (wish 7) and **README media as GIFs** (wishes 2 and 3).
- *Later*: **Lab preview in the launcher** (wish 1) and **Tour video for YouTube** (wish 4). *Reproducible Dotio captures and a new tour video* becomes **Reproducible Dotio captures**, without the render.
- *Ideas*: **Evolution lab** (wish 5, with the agent's approach in steps) and **Repositories per lab** (wish 6, with the advice).

Kinetra's [roadmap](../../external/Kinetra/docs/roadmap.md): **Leaner agentic cycle** first in *Next*, **Shared UI in the README** at its end, depending on Lab's *Demo scenario menu*, and **Images in the launcher menu** in *Later*.

## Affected areas

`docs/roadmap.md`, this document, the design index, and the item's name in `tools/capture/README.md`. In Kinetra, on a branch of the same name: `docs/roadmap.md`, its design document and its design index.

## Test plan

Documentation only: the Docs level in both repositories.

## Review

Checked the diff against the user's seven wishes: each is an item, in Dutch intent translated to English. Searched both repositories for the old item name (`Reproducible Dotio captures and a new tour video`): the capture README followed; the two design documents that name it are records of their time and stay. Checked the facts in the items against the code (`launcher/src/applications.cpp`, `include/kinetra/runtime/launcher.h`), GitHub's pull request list and the clone's objects.

## Verification

- `python3 external/Kinetra/tools/check_docs.py`: all links resolve.
- In Kinetra: `python3 tools/check_docs.py`: all links resolve.

## Pull request

**docs(roadmap): GIFs in both READMEs, a Demo scenario menu, a launcher preview, an evolution lab and a leaner cycle**

- **Summary:** the user's wishes of 2026-11-23 as roadmap items in Lab and Kinetra.
- **Changes:** Lab roadmap (five new items, two ideas, one item without the tour render, the agentic cycle first); the item's name in the capture README; this design document and the index.
- **Design:** this document and the [Kinetra design](https://github.com/SMB-M87/Kinetra/blob/main/docs/design/2026-11-23-roadmap-media-labs-cycle.md).
- **Verification:** Docs level in both repositories.
- **Review notes:** the positions follow the user's answers of 2026-11-23; the other new items stand at the end of their sections.
- **Dependencies and merge order:** the Kinetra pull request of the same branch merges first, with a merge commit; then a pointer-only pull request records it.
- **Follow-up:** the items themselves.

## Open points

- Resolved on 2026-11-23: the cycle goes first, the Demo scenario menu before Kinetra's GIFs, the tour video waits for *Next* and *Later* only and is rendered with `capture_tour.sh`.
- The positions of *Lab preview in the launcher*, *Tour video for YouTube* and the two ideas are still the agent's; the user can move them.
