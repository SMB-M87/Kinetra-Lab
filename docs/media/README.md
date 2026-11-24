# Screenshots, clips and demo video

Real application captures behind the project's README gallery, what each one shows, and how to refresh them. The gallery opens with the tour's thumbnail and shows the clips and the launcher screenshots; the full-size application screenshots are listed here. Every screenshot, clip and the tour video come from [`kinetra_capture`](../../tools/capture/README.md), which renders the real sessions offscreen with a fixed seed and scripted input. The same tool renders the shared-UI GIFs of Kinetra's README into Kinetra's [media guide](../../external/Kinetra/docs/media/README.md).

## Table of contents

- [Files](#files)
- [Capture details](#capture-details)
- [Which captures a change makes stale](#which-captures-a-change-makes-stale)
- [Refreshing the media](#refreshing-the-media)

[Parent guide / documentation index](../README.md) · [Project overview](../../README.md#screenshots-and-demo)

## Files

| Files | Contents |
| --- | --- |
| [launcher.png](launcher.png), [launcher-difficulty.png](launcher-difficulty.png) | Application menu with DEMO highlighted and its preview visible, and the Dotio difficulty page with Medium |
| [launcher-demo-scenarios.png](launcher-demo-scenarios.png) | Demo configuration page with Terrain highlighted |
| [launcher-loadscreen.png](launcher-loadscreen.png) | i4sim loadscreen: the first catalog page, SHOWCASE highlighted |
| [demo.png](demo.png), [demo-debug.png](demo-debug.png) | Demo with Debug Mode off, and on with its default layers and the debug menu open on the MOTION section: the steering values that can be tuned while the scene runs |
| [demo-gravity.png](demo-gravity.png), [demo-maze.png](demo-maze.png), [demo-doorway.png](demo-doorway.png) | Fixed Demo layouts for gravity and contacts, maze routes, and two groups approaching a shared opening |
| [demo-clip.gif](demo-clip.gif), [demo-debug-clip.gif](demo-debug-clip.gif) | Demo: the player walks to two clicked points among the agents, which wander by steering around their homes, in both views; the debug clip has the menu closed |
| [dotio.png](dotio.png), [dotio-debug.png](dotio-debug.png) | Dotio gameplay, and Debug Mode with every layer on and the debug menu open on LAYERS: PATHS with its four parts in the colors of their AI states, zoomed out |
| [dotio-settings.png](dotio-settings.png) | Dotio's settings panel (STATISTICS, DEBUG MODE, FRAME TIMES, FRAME LIMIT) with SIM CONTROLS expanded |
| [dotio-clip.gif](dotio-clip.gif) | Dotio: the player runs a square through the corridors and shoots in bursts at the nearest enemy in sight |
| [dotio-debug-clip.gif](dotio-debug-clip.gif) | Dotio's AI from a wide view, twelve seconds with every debug layer on and the menu closed. Enemies that wander show the three feelers they steer by. The player holds its ground and shoots back; the clip starts when an enemy has spent its ammunition and runs for shelter (RECOVER, green), and shows the reload out of sight and the return (PURSUE, then ATTACK) |
| [i4sim.png](i4sim.png), [i4sim-debug.png](i4sim-debug.png) | i4sim `4320x2400` (29 movers, 18 producers) running with the Reciprocal profile and plain A*, with Debug Mode off, and on with the LAYERS section, every part and its swatch |
| [i4sim-clip.gif](i4sim-clip.gif), [i4sim-debug-clip.gif](i4sim-debug-clip.gif) | The same floor and settings for twelve seconds at the session's 200 ticks per second, in both views; the debug clip has the menu closed |
| [i4sim-showcase-junction-crossing-clip.gif](i4sim-showcase-junction-crossing-clip.gif), [i4sim-showcase-corridor-passing-clip.gif](i4sim-showcase-corridor-passing-clip.gif), [i4sim-showcase-corridor-recovery-clip.gif](i4sim-showcase-corridor-recovery-clip.gif), [i4sim-showcase-predictive-passing-clip.gif](i4sim-showcase-predictive-passing-clip.gif), [i4sim-showcase-h-crossing-clip.gif](i4sim-showcase-h-crossing-clip.gif) | The five i4sim [showcase scenes](../../labs/i4sim/blueprints/README.md#showcase-scenes), each one full cycle in Debug Mode at 200 ticks per second, so each lasts as long as its scene's cycle |
| [kinetra-lab-tour-thumbnail.png](kinetra-lab-tour-thumbnail.png) | The README's picture of the tour, linked to the [YouTube video](https://youtu.be/jkPwx831MOU): four moments of the video (the launcher, Demo, Dotio and i4sim) in a 2 × 2 mosaic at 1280 × 720, with a play button. A picture and not the video, because GitHub plays a video in a README only as an uploaded attachment of at most 10 MB on a free plan |

## Capture details

- **Rendering.** SDL's offscreen driver with software Mesa, read back with `glReadPixels`; no window decoration.
- **Sizes.** Screenshots 1920 × 1080. Clips are rendered at 960 × 540, so the bitmap font stays sharp, and play at 15 frames per second: four seconds for Demo and the Dotio gameplay clip, twelve for the Dotio debug clip and the i4sim clips. A showcase clip lasts one cycle of its scene instead, which the tool measures before recording, so the GIF loops as the application repeats the scene.
- **Sessions.** Created as the bundled launcher creates them, with seed 42, 48 Demo agents and Dotio on Medium. The debug scenes open the debug menu with Shift+Ctrl, which in Dotio also switches Debug Mode on. Every step advances 1/60 s, so the statistics show 60 frames per second. Demo runs 2 s before a capture and Dotio 8 s, for its enemies to arrive. The showcase scenes start at once, as after Space on the SHOWCASE page.
- **Dotio's player.** Scripted against the world of the running session, so it reacts to the enemies it meets: it aims at the nearest enemy on screen that a bolt can reach, with no wall or crate in between, ahead of one that moves, and holds fire without one. A screenshot is taken while it shoots at an enemy. The debug clip zooms out nine wheel notches and waits, without firing, until an enemy enters Recover (60 s at most).
- **i4sim's settings.** The scripts choose PROFILE RECIPROCAL and PLANNER A* in the debug menu's MOTION section before Space; the traffic policies stay off. The clips show the twelve seconds of the floor's first 150 s in which most movers drive, which the tool finds in a headless run before it records; the screenshots are taken halfway through them. Why this floor and these settings is in the [design document](../design/2026-11-19-credible-media-clips.md#decisions).
- **GIFs.** One palette per clip without dithering, and only the changed rectangle per frame; about 5.1 MB for the six application clips and 2.5 MB for the five showcase clips.
- **Reproducibility.** A refresh gives the same files, Dotio's included: its randomness comes from the seed since roadmap phase 6 ([Dotio restructuring](../design/2026-11-21-dotio-restructuring.md)).
- **What each script does** is in the [capture tool](../../tools/capture/README.md#how-it-works).

## Which captures a change makes stale

The capture script scales the three main lab screenshots down for the launcher. If the requested scenes include `launcher` or `ui-launcher`, it rebuilds the capture tool after updating the stills and records those scenes with the new images.

A pull request that changes what a capture shows refreshes it in the same pull request ([rule](../../external/Kinetra/AGENTS.md#repository-documentation)). Refresh the captures of every row the change touches and commit no other: a refresh on another machine can change a few pixels of a GIF without a reason, through another Mesa or ffmpeg. The shared-UI GIFs of Kinetra's README have their own [table](../../external/Kinetra/docs/media/README.md#which-gifs-a-change-makes-stale).

| A change to | Makes stale |
| --- | --- |
| Demo (`labs/demo/`): its scene, drawing, rows or tuning | `demo`, `demo-debug`, `demo-gravity`, `demo-maze`, `demo-doorway`, `demo-clip`, `demo-debug-clip`; its scenario list also `launcher-demo-scenarios` |
| Dotio (`labs/dotio/`) | `dotio`, `dotio-debug`, `dotio-settings`, `dotio-clip`, `dotio-debug-clip`; its difficulties also `launcher-difficulty` |
| i4sim (`labs/i4sim/`, its blueprints) | `i4sim`, `i4sim-debug`, `i4sim-clip`, `i4sim-debug-clip`; a showcase blueprint its `i4sim-showcase-*-clip`; the loadscreen or catalog also `launcher-loadscreen` |
| The launcher's registrations (`launcher/`) | `launcher`, `launcher-demo-scenarios`, `launcher-difficulty`, `launcher-loadscreen` |
| The shared Lab policy (`labs/shared/`): settings rows, debug rows, inspector layout | The captures that show the changed panel: `dotio-settings` for the settings rows, the `*-debug` screenshots for the debug menu |
| A capture script (`tools/capture/`) | The scenes whose script changed; a change to `Stage` or the GIF step all captures |
| Kinetra's overlays or drawing (`external/Kinetra`) | The captures that show the changed part, as in Kinetra's table, here and there |

## Refreshing the media

```sh
cmake --preset graphics
cmake --build --preset graphics --target kinetra_capture
tools/capture/capture_media.sh
```

The script needs ffmpeg for the GIFs. It writes every PNG and GIF into this directory, and the shared-UI GIFs into `external/Kinetra/docs/media`; `tools/capture/capture_media.sh build-graphics <scene>...` refreshes only some, as the table above names them. Look at every refreshed file before committing it: a script that clicks a menu row by number shows the wrong row when the menu changes. Keep the filenames synchronized with this guide and, for the clips, with the [README gallery](../../README.md#screenshots-and-demo), and the shared-UI GIFs with Kinetra's media guide and README; `kinetra_capture_tests` checks all of them.

The tour video has its own script, because it renders for minutes:

```sh
cmake --build --preset graphics --target kinetra_capture
tools/capture/capture_tour.sh
```

It needs ffmpeg with libx264 and writes `.docs/kinetra-lab-tour.mp4`, in a folder Git ignores: H.264 with `yuv420p` and MP4 fast start, for broad playback support, without an audio track. The video goes to YouTube and not into the repository, where every render stayed in the history of each clone (the user's decision of 2026-11-23; `.gitignore` keeps `*.mp4` out). Then it runs `tools/capture/tour_thumbnail.sh`, which writes `kinetra-lab-tour-thumbnail.png` here from the new video; run that script alone to refresh only the thumbnail, from a video in `.docs/` that `capture_tour.sh` rendered or from a video passed as its first argument. Dotio's part differs per run. Look at frames from every chapter before uploading it; a click that went wrong shows as a scene that does not match its caption. Look at the thumbnail too: its four moments are fixed times in the tour, so a changed chapter can move another scene into a tile. The video is uploaded by hand when the user asks for a new one; then replace the YouTube link in the README and in the thumbnail's row above.
