# Screenshots and demo video

Real application captures used by the project's README gallery, what each one shows, and how to refresh them.

## Table of contents

- [Files](#files)
- [Capture details](#capture-details)
- [Refreshing the media](#refreshing-the-media)

[Parent guide / documentation index](../README.md) · [Project overview](../../README.md#screenshots-and-demo)

## Files

| Files | Contents |
| --- | --- |
| [launcher.png](launcher.png), [launcher-difficulty.png](launcher-difficulty.png) | Application menu and Dotio difficulty selection |
| [launcher-loadscreen.png](launcher-loadscreen.png) | i4sim loadscreen with blueprint selection |
| [demo.png](demo.png), [demo-debug.png](demo-debug.png) | Demo with debug layers disabled and enabled |
| [dotio.png](dotio.png), [dotio-debug.png](dotio-debug.png) | Dotio with debug layers disabled and enabled |
| [dotio-settings.png](dotio-settings.png) | Dotio settings with the simulation controls expanded |
| [kinetra-lab-tour.mp4](kinetra-lab-tour.mp4) | Tour video: 2 min 14 s, H.264 at 1920 × 1080 and 60 fps, with an AAC audio track |
| [i4sim.png](i4sim.png), [i4sim-debug.png](i4sim-debug.png) | i4sim with debug mode disabled and enabled |

## Capture details

The PNG files are window captures of about 1915 × 1055 pixels; the exact size varies by a few pixels per file. Seeds and session options are not recorded, except what a capture shows on screen.

- **Demo:** `demo-debug.png` has every visual layer enabled (vectors, steering probes, paths, collision shapes, active agents); its statistics show 71 of 193 agents active. The two Demo captures are not the same scene.
- **Dotio:** `dotio-debug.png` has Debug Mode with vectors, AI states, paths and their filters, steering probes, collision shapes, active agents, hiding hits and Invulnerable enabled.
- **i4sim:** both captures show the `MAS-demonstrator` blueprint (`MAS-demonstrator.json`), seed 42, paused at 200 UPS, at tick 83 (`i4sim.png`) and tick 116 (`i4sim-debug.png`, with the LAYERS section of the debug menu open and every layer and part on). `i4sim-debug.png` is rendered offscreen like the loadscreen below, at 1915 × 1055.
- **i4sim loadscreen:** `launcher-loadscreen.png` shows the first catalog page with the SHOWCASE row first and highlighted, then `1680x960+` to `8640x4800`. Unlike the other captures it is not a window capture: it was rendered offscreen (`SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1`) at 1915 × 1054 from a fresh i4sim session and read back with `glReadPixels`, so it has no window decoration and RGB instead of RGBA pixels.

## Refreshing the media

Rebuild the current checkout using the [root verification commands](../../README.md#tests-and-resources), then run the applications with the [Demo controls](../../labs/demo/README.md#controls), [Dotio controls](../../labs/dotio/README.md#launch-and-controls) and [i4sim controls](../../labs/i4sim/README.md#controls). The bundled launcher picks a new world seed per run and prints it (`World seed: …`); record it next to the capture.

For i4sim, `./build/kinetra_i4sim --blueprint labs/i4sim/blueprints/MAS-demonstrator.json` opens the same blueprint directly with seed 42 (the default; `--seed N` changes it), paused. Without `--blueprint` it opens the loadscreen; inside the bundled launcher, choose i4sim.

Keep filenames synchronized with the [README gallery](../../README.md#screenshots-and-demo). Encode video as H.264 with `yuv420p` and MP4 fast start for broad playback support.
