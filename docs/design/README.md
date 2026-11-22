# Lab design documents

One design document per Lab branch, written before the code and kept after merging. The format, lifecycle and template are shared with Kinetra; see [Kinetra design documents](../../external/Kinetra/docs/design/README.md#template). Changes that touch both repositories get a document in each, and the Lab document links to the Kinetra one.

## Table of contents

- [Index](#index)

[Parent guide](../README.md) · [Project overview](../../README.md) · [Documentation index](../README.md) · [Workflow](../../external/Kinetra/docs/workflow.md)

## Index

| Date | Document | Status |
| --- | --- | --- |
| 2026-11-22 | [Demo agent inspector](2026-11-22-demo-agent-inspector.md) | implemented |
| 2026-11-22 | [Agent inspector overlay](2026-11-22-agent-inspector-overlay.md) | implemented |
| 2026-11-22 | [Skip the release build when the zip cannot change](2026-11-22-release-skip-docs.md): a push to `main` with only Markdown, docs or skills, in Lab and the Kinetra range, publishes nothing new | implemented |
| 2026-11-22 | [Pointer-only pull requests in Lab](2026-11-22-pointer-only-ci.md): CI runs only the docs check when the pointer brings no engine change; a pointer move is its own pull request | implemented |
| 2026-11-21 | [One prioritized roadmap](2026-11-21-roadmap-priorities.md): the quality roadmap closed, every open point and the user's new wishes on the roadmap in order of priority | implemented |
| 2026-11-21 | [Roadmap as a backlog](2026-11-21-roadmap-backlog.md): `docs/roadmap.md` with the work left after the quality roadmap and the user's ideas | implemented |
| 2026-11-21 | [Demo restructuring](2026-11-21-demo-restructuring.md): roadmap phase 5, bodies out of headers, scene operations, contact tests to Kinetra, module README | implemented |
| 2026-11-21 | [i4sim passing under pressure](2026-11-21-i4sim-passing-under-pressure.md): roadmap phase 7, causes from replayed runs first, then guarded passing (Reciprocal gains with passing on; off by default) and the standstill near a destination, documented as a limit | implemented |
| 2026-11-21 | [Dotio restructuring](2026-11-21-dotio-restructuring.md): roadmap phase 6, seeded statistics baseline first, routes on Kinetra's weighted grid | implemented |
| 2026-11-21 | [Windows release zip](2026-11-21-windows-release-zip.md): a ready-to-run zip with the i4sim blueprints on every merge on `main` and every tag `v*` | implemented |
| 2026-11-21 | [HTTPS submodule by default](2026-11-21-https-submodule.md): clone without an SSH key; `pushInsteadOf` for SSH pushes | implemented |
| 2026-11-20 | [Run Lab CI on pull requests](2026-11-20-pr-only-ci.md) | implemented |
| 2026-11-20 | [Agent terminology in Lab documentation](2026-11-20-agent-terminology.md) | implemented |
| 2026-11-20 | [i4sim motion changes keep routes](2026-11-20-i4sim-tuning-stall.md): only PLANNER changes replan; SPEED, FORCE and HORIZON steps no longer strand movers | implemented |
| 2026-11-20 | [Frame pacing in WSLg](2026-11-20-wslg-frame-pacing.md): why the Linux build stutters in WSLg and the Windows build does not; three options, decision open | draft |
| 2026-11-20 | [Record Kinetra main after the navigation rebuild fix](2026-11-20-record-kinetra-navigation-rebuild.md): `7d3e586` instead of `92db1b6` | implemented |
| 2026-11-20 | [MAS demonstrator as a test fixture](2026-11-20-mas-test-fixture.md): out of the catalog, into `labs/i4sim/tests/`, so `main` passes again | implemented |
| 2026-11-20 | [Record Kinetra main after the include layout](2026-11-20-record-kinetra-include-layout.md): `92db1b6` instead of its branch commit | implemented |
| 2026-11-20 | [Include layout](2026-11-20-include-layout.md): every module on one `include/<name>/` and `src/`, topic folders, namespaces equal to include roots | implemented |
| 2026-11-20 | [Local verification runs only graphics](2026-11-20-verify-locally-graphics.md): `ci`, `headless` and Windows from the pull request's CI | implemented |
| 2026-11-20 | [Tour version note](2026-11-20-tour-version-note.md): below the thumbnail, which copy of the tour is newest instead of a second YouTube link | implemented |
| 2026-11-20 | [Tour thumbnail in the README](2026-11-20-tour-thumbnail.md): a clickable mosaic of the tour at the top of the gallery, generated from the video | implemented |
| 2026-11-19 | [Generated tour video](2026-11-19-tour-video.md): the capture tool plays the launcher and all three applications with a visible pointer, keycaps and captions | implemented |
| 2026-11-19 | [H crossing without stations](2026-11-19-h-crossing-without-stations.md): the unused stations out of the H showcase scene | implemented |
| 2026-11-19 | [Debug workbench](2026-11-19-debug-workbench.md): tree debug menus with live tuning in Demo and Dotio, steering wander, Shift+Ctrl, zoom in the debug menu, FRAME LIMIT | implemented |
| 2026-11-19 | [Record the Kinetra corner slide fix](2026-11-19-record-kinetra-corner-slide.md): the pointer back on a Kinetra `main` with the corner slide fix, so i4sim passes again | implemented |
| 2026-11-19 | [README icon](2026-11-19-readme-icon.md): the Kinetra icon at the top of the README | implemented |
| 2026-11-19 | [Credible media clips](2026-11-19-credible-media-clips.md): a scripted Dotio player that aims at enemies, and i4sim captures of a larger floor with the Reciprocal profile | implemented |
| 2026-11-19 | [i4sim path planning off and a smooth H crossing](2026-11-19-i4sim-h-crossing-motion.md): PLANNER OFF, motion parameters in scenarios, measured H settings | implemented |
| 2026-11-19 | [i4sim showcase clips](2026-11-19-i4sim-showcase-clips.md): one looping GIF per showcase scene, each one full cycle | implemented |
| 2026-11-19 | [Media capture tool](2026-11-19-media-capture.md): `kinetra_capture` renders every screenshot and six GIF clips offscreen | implemented |
| 2026-11-19 | [Record Kinetra main after the debug menu steppers](2026-11-19-kinetra-main-pointer.md): `4af2d97` instead of its branch commit | implemented |
| 2026-11-19 | [i4sim debug menu](2026-11-19-i4sim-debug-menu.md): sections, lists, `[-] [+]` buttons and layer parts | implemented |
| 2026-11-19 | [i4sim showcase shuttle](2026-11-19-i4sim-showcase-shuttle.md): showcase scenes repeat by shuttling or restarting; seven city blueprints removed | implemented |
| 2026-11-19 | [Record Kinetra main](2026-11-19-submodule-kinetra-main.md): the `external/Kinetra` pointer on Kinetra's `main` merge commit | implemented |
| 2026-11-19 | [build.sh builds Windows](2026-11-19-build-sh-windows.md): the MinGW cross build when its tools are installed | implemented |
| 2026-11-19 | [Smooth rendering and frame times](2026-11-19-smooth-rendering.md): draw between fixed ticks in all three applications, FRAME TIMES graph | implemented |
| 2026-11-19 | [i4sim showcase scenes](2026-11-19-i4sim-showcase.md): a `scenario` section in blueprints and a SHOWCASE page on the loadscreen | implemented |
| 2026-11-19 | [Window icon](2026-11-19-window-icon.md): the Kinetra icon on every window and Windows application executable | implemented |
| 2026-11-19 | [Native Windows build](2026-11-19-windows-native-build.md): build on Windows in MSYS2 or start the WSL cross build from PowerShell | implemented |
| 2026-11-19 | [Submodule detached HEAD](2026-11-19-submodule-detached-head.md): put `external/Kinetra` on Kinetra main and record the pointer | implemented |
| 2026-11-19 | [Windows build with MinGW-w64](2026-11-19-windows-mingw.md): cross build from WSL, Windows CI job | implemented |
| 2026-11-19 | [Static window titles](2026-11-19-static-window-titles.md): "Launcher" or the application name, state through `status()` | implemented |
| 2026-11-19 | [Blueprint copy race](2026-11-19-blueprint-copy-race.md): copy the i4sim catalog once per build | implemented |
| 2026-11-19 | [Link the architecture decisions](2026-11-19-adr-index-link.md) from the Lab documentation index | implemented |
| 2026-11-19 | [i4sim restructuring](2026-11-19-i4sim-restructuring.md): roadmap phase 4, passing safety net first | implemented |
| 2026-11-18 | [Safety net and shared presentation](2026-11-18-safety-net-shared-presentation.md): roadmap phase 3, characterization tests first | implemented |
| 2026-11-18 | [Quality roadmap](2026-11-18-quality-roadmap.md): phases for tooling, tests, shared presentation, i4sim, Demo, Dotio, predictive passing and documentation cleanup; closed on 2026-11-21, the rest is on the [roadmap](../roadmap.md) | implemented |
| 2026-11-18 | [Tooling and CI](2026-11-18-tooling-and-ci.md): roadmap phase 2 | implemented |
| 2026-11-18 | [Development process and agent rules](2026-11-18-development-process.md) | implemented |
