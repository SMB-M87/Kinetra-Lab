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
| [dotio-clip.gif](dotio-clip.gif), [dotio-debug-clip.gif](dotio-debug-clip.gif) | Dotio: the player runs a square through the corridors and fires in bursts, in both views |
| [i4sim.png](i4sim.png), [i4sim-debug.png](i4sim-debug.png) | i4sim `MAS-demonstrator` running, with Debug Mode off, and on with the LAYERS section, every part and its swatch |
| [i4sim-clip.gif](i4sim-clip.gif), [i4sim-debug-clip.gif](i4sim-debug-clip.gif) | i4sim at 800 ticks per second, in both views; the debug clip shows the SIMULATION section |
| [kinetra-lab-tour.mp4](kinetra-lab-tour.mp4) | Tour video: 2 min 14 s, H.264 at 1920 × 1080 and 60 fps, with an AAC audio track; recorded by hand |

## Capture details

- **Rendering.** SDL's offscreen driver with software Mesa, read back with `glReadPixels`; no window decoration.
- **Sizes.** Screenshots 1920 × 1080. Clips are rendered at 960 × 540, so the bitmap font stays sharp, and play at 15 frames per second for four seconds.
- **Sessions.** Created as the bundled launcher creates them, with seed 42, 48 Demo agents and Dotio on Medium. Every step advances 1/60 s, so the statistics show 60 frames per second. Demo and Dotio run 2 s before a capture (Dotio 8 s, for its enemies to arrive), i4sim 20 s.
- **GIFs.** One palette per clip without dithering, and only the changed rectangle per frame; about 2 MB for all six.
- **Reproducibility.** A refresh gives the same files, except Dotio's: its AI still uses unseeded randomness ([quality roadmap](../design/2026-11-18-quality-roadmap.md)).
- **What each script does** is in the [capture tool](../../tools/capture/README.md#how-it-works).

## Refreshing the media

```sh
cmake --preset graphics
cmake --build --preset graphics --target kinetra_capture
tools/capture/capture_media.sh
```

The script needs ffmpeg for the GIFs. It writes every PNG and GIF into this directory; `tools/capture/capture_media.sh build-graphics <scene>...` refreshes only some. Look at every refreshed file before committing it: a script that clicks a menu row by number shows the wrong row when the menu changes. Keep the filenames synchronized with the [README gallery](../../README.md#screenshots-and-demo).

The tour video is not generated. Encode a new one as H.264 with `yuv420p` and MP4 fast start for broad playback support.
