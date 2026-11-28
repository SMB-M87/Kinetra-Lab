# Sharper launcher previews

Status: accepted · Branch: `ccr-3ae7a52c-qpwl3u` · Related: [launcher preview stills of their own](2026-11-27-launcher-preview-stills.md), [Lab previews in the launcher](../../external/Kinetra/docs/design/2026-11-24-launcher-images.md), [capture tool](../../tools/capture/README.md), [media guide](../media/README.md)

The launcher shows its Demo, Dotio and i4sim stills larger than they are embedded, so all three look blurry. The stills grow from 320 × 180 to 480 × 270, close to the size the launcher draws them at, and Dotio gets a preview scene of its own, as Demo and i4sim did on 2026-11-27.

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

- **Stretched stills.** The launcher fits a still into an image slot of 250 × 140 units (`external/Kinetra/src/runtime/launcher.cpp`, `image_slot`), at a menu scale of at most 2. With the three applications listed, the scale is 2 at 1920 × 1080, 1280 × 800 and 1280 × 720 (`MenuLayout`), so a 16:9 still is drawn at about 498 × 280 pixels. A 320 × 180 still is stretched 1.56 times with linear filtering, which blurs every edge and the bitmap text.
- **Dotio's still.** It is the 1920 × 1080 screenshot `dotio.png` reduced six times (`tools/capture/capture_media.sh`). Its player, enemies and bolts become a few pixels, and the SETTINGS button and the HEALTH, MANA and BULLET SPEED bars become smudges.

## Goals and non-goals

- Goals: all three stills sharp at the size the launcher draws them; a Dotio still whose player and enemies read clearly; all three reproducible with the capture tool, as before.
- Non-goals: how the launcher fits, filters or embeds an image, which is Kinetra's; Demo's and i4sim's preview scenes, which stay as they are; Dotio's HUD, which the still keeps, because no input hides it; the shared-UI GIF `ui-launcher` in Kinetra, which follows a Lab-only change with the next Kinetra change that makes it stale ([Kinetra's media guide](../../external/Kinetra/docs/media/README.md#which-gifs-a-change-makes-stale)).

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Still size | 480 × 270 (the user's choice of 2026-11-28) | Drawn at about 498 × 280 it is stretched only 1.04 times, nearly one pixel per pixel. Demo's and i4sim's 960 × 540 previews are exactly twice its size. 640 × 360 would always be reduced, but its preview scenes would need another window size and framing, and its header four times the current one |
| Source of Dotio's still | A scene of its own, `dotio-preview` (the user's choice of 2026-11-28) | As for Demo and i4sim: the main screenshot is made to be seen at full size |
| Dotio's view | A 1920 × 1080 window in Debug Mode, eight wheel notches in, to Dotio's closest zoom of 50 pixels per unit, the scripted player as in `dotio`, reduced four times | The same area as a 960 × 540 window at Dotio's starting zoom, with shapes as large, but the HUD, whose scale stops at 2, covers a quarter of the share: in a 960 × 540 window its bars filled 40 % of the still's width. Only Debug Mode lets the wheel zoom (`labs/dotio/src/presentation/session.cpp`); it adds only the DEBUG button next to SETTINGS, as in Demo's still |
| Changing the view | Through input only: DEBUG MODE in the settings panel and the wheel | The tool's [invariant](../../tools/capture/README.md#invariants-and-contracts) |

## Design

- `launcher/EmbedPreviews.cmake` embeds 480 × 270 RGBA stills; `capture_media.sh` scales the preview scenes to that size and takes Dotio's still from `dotio-preview`.
- `scenes.cpp`: `preview()` takes a window size, 960 × 540 unless given. `dotio_preview` starts Dotio, turns DEBUG MODE on, turns the wheel `dotio_preview_zoom_in` notches, warms up and waits for a shot as `dotio` does, in a 1920 × 1080 window.

## Affected areas

- `launcher/EmbedPreviews.cmake`, `launcher/previews/*.rgba`, `launcher/tests/menu_tests.cpp`, `launcher/README.md`.
- `tools/capture/src/scenes/scenes.cpp`, `tools/capture/capture_media.sh`, `tools/capture/tests/capture_tests.cpp`, `tools/capture/README.md`.
- `docs/media/dotio-preview.png` (new), `docs/media/launcher.png`, `docs/media/README.md`.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| Each registration carries a 480 × 270 still, a different one per lab | `kinetra_menu:` registrations case, with the new size | Targeted |
| Three previews, each a screenshot at a whole multiple of the still | `kinetra_capture_tests` *a launcher preview is a screenshot at a whole multiple of the launcher's still*, for 480 × 270 and three previews | Targeted |
| `dotio-preview` exists once, with a file and a row in the media guide | The scene-list and gallery cases of `kinetra_capture_tests` | Targeted |
| The stills embed and look sharp in the launcher | `capture_media.sh build-graphics demo-preview dotio-preview i4sim-preview launcher`; look at the three previews, the stills, and `launcher.png` with each lab highlighted at 1920 × 1080 | Targeted |
| The build still embeds the larger header in reasonable time | Compile `launcher/src/applications.cpp` without ccache before and after | Targeted |
| The tool runs offscreen | `kinetra_capture_smoke` | Full (CI) |

## Review

## Verification

## Pull request

## Open points

- `ui-launcher` in Kinetra still shows the 320 × 180 stills until the next Kinetra change that makes it stale refreshes it.
