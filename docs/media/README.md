# Screenshots, clips and demo video

Real application captures used by the project's README gallery, what each one shows, and how to refresh them. Every screenshot and clip comes from [`kinetra_capture`](../../tools/capture/README.md), which renders the real sessions offscreen with a fixed seed and scripted input.

## Table of contents

- [Files](#files)
- [Capture details](#capture-details)
- [Refreshing the media](#refreshing-the-media)

[Parent guide / documentation index](../README.md) · [Project overview](../../README.md#screenshots-and-demo)

## Files

| Files | Contents |
| --- | --- |
| [launcher.png](launcher.png), [launcher-difficulty.png](launcher-difficulty.png) | Application menu with DEMO highlighted, and the Dotio difficulty page with Medium |
| [launcher-loadscreen.png](launcher-loadscreen.png) | i4sim loadscreen: the first catalog page, SHOWCASE highlighted |
| [demo.png](demo.png), [demo-debug.png](demo-debug.png) | Demo with Debug Mode off, and on with the debug menu open and every visual layer |
| [demo-clip.gif](demo-clip.gif), [demo-debug-clip.gif](demo-debug-clip.gif) | Demo: the player walks to two clicked points among the agents, in both views |
| [dotio.png](dotio.png), [dotio-debug.png](dotio-debug.png) | Dotio gameplay, and Debug Mode with every visual row of the debug menu, zoomed out |
| [dotio-settings.png](dotio-settings.png) | Dotio's settings panel with SIM CONTROLS expanded |
| [dotio-clip.gif](dotio-clip.gif) | Dotio: the player runs a square through the corridors and shoots in bursts at the nearest enemy in sight |
| [dotio-debug-clip.gif](dotio-debug-clip.gif) | Dotio's AI from a wide view, twelve seconds with every visual debug row on and the menu closed. The player holds its ground and shoots back; the clip starts when an enemy has spent its ammunition and runs for shelter (RECOVER, green), and shows the reload out of sight and the return (PURSUE, then ATTACK) |
| [i4sim.png](i4sim.png), [i4sim-debug.png](i4sim-debug.png) | i4sim `4320x2400` (29 movers, 18 producers) running with the Reciprocal profile and plain A*, with Debug Mode off, and on with the LAYERS section, every part and its swatch |
| [i4sim-clip.gif](i4sim-clip.gif), [i4sim-debug-clip.gif](i4sim-debug-clip.gif) | The same floor and settings for twelve seconds at the session's 200 ticks per second, in both views; the debug clip has the menu closed |
| [i4sim-showcase-junction-crossing-clip.gif](i4sim-showcase-junction-crossing-clip.gif), [i4sim-showcase-corridor-passing-clip.gif](i4sim-showcase-corridor-passing-clip.gif), [i4sim-showcase-corridor-recovery-clip.gif](i4sim-showcase-corridor-recovery-clip.gif), [i4sim-showcase-predictive-passing-clip.gif](i4sim-showcase-predictive-passing-clip.gif), [i4sim-showcase-h-crossing-clip.gif](i4sim-showcase-h-crossing-clip.gif) | The five i4sim [showcase scenes](../../labs/i4sim/blueprints/README.md#showcase-scenes), each one full cycle in Debug Mode at 200 ticks per second, so each lasts as long as its scene's cycle |
| [kinetra-lab-tour.mp4](kinetra-lab-tour.mp4) | Tour video: 2 min 14 s, H.264 at 1920 × 1080 and 60 fps, with an AAC audio track; recorded by hand |

## Capture details

- **Rendering.** SDL's offscreen driver with software Mesa, read back with `glReadPixels`; no window decoration.
- **Sizes.** Screenshots 1920 × 1080. Clips are rendered at 960 × 540, so the bitmap font stays sharp, and play at 15 frames per second: four seconds for Demo and the Dotio gameplay clip, twelve for the Dotio debug clip and the i4sim clips. A showcase clip lasts one cycle of its scene instead, which the tool measures before recording, so the GIF loops as the application repeats the scene.
- **Sessions.** Created as the bundled launcher creates them, with seed 42, 48 Demo agents and Dotio on Medium. Every step advances 1/60 s, so the statistics show 60 frames per second. Demo runs 2 s before a capture and Dotio 8 s, for its enemies to arrive. The showcase scenes start at once, as after Space on the SHOWCASE page.
- **Dotio's player.** Scripted against the world of the running session, because the AI is not seeded: it aims at the nearest enemy on screen with a clear line of sight, ahead of one that moves, and holds fire without one. The debug clip zooms out nine wheel notches and waits, without firing, until an enemy enters Recover (60 s at most).
- **i4sim's settings.** The scripts choose PROFILE RECIPROCAL and PLANNER A* in the debug menu's MOTION section before Space; the traffic policies stay off. The floor runs 116 s first: with seed 42 the twelve seconds after that are its busiest, with 12 to 18 of the 29 movers driving. Why this floor and these settings is in the [design document](../design/2026-11-19-credible-media-clips.md#decisions).
- **GIFs.** One palette per clip without dithering, and only the changed rectangle per frame; about 5.2 MB for the six application clips and 2.7 MB for the five showcase clips.
- **Reproducibility.** A refresh gives the same files, except Dotio's: its AI still uses unseeded randomness ([quality roadmap](../design/2026-11-18-quality-roadmap.md)). Render the Dotio debug clip a few times and keep a take in which the enemies stay around the player.
- **What each script does** is in the [capture tool](../../tools/capture/README.md#how-it-works).

## Refreshing the media

```sh
cmake --preset graphics
cmake --build --preset graphics --target kinetra_capture
tools/capture/capture_media.sh
```

The script needs ffmpeg for the GIFs. It writes every PNG and GIF into this directory; `tools/capture/capture_media.sh build-graphics <scene>...` refreshes only some. Look at every refreshed file before committing it: a script that clicks a menu row by number shows the wrong row when the menu changes. Keep the filenames synchronized with the [README gallery](../../README.md#screenshots-and-demo).

The tour video is not generated. Encode a new one as H.264 with `yuv420p` and MP4 fast start for broad playback support.
