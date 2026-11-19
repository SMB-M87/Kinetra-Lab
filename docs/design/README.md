# Lab design documents

One design document per Lab branch, written before the code and kept after merging. The format, lifecycle and template are shared with Kinetra; see [Kinetra design documents](../../external/Kinetra/docs/design/README.md#template). Changes that touch both repositories get a document in each, and the Lab document links to the Kinetra one.

## Table of contents

- [Index](#index)

[Parent guide](../README.md) · [Project overview](../../README.md) · [Documentation index](../README.md) · [Workflow](../../external/Kinetra/docs/workflow.md)

## Index

| Date | Document | Status |
| --- | --- | --- |
| 2026-11-19 | [Media capture tool](2026-11-19-media-capture.md): `kinetra_capture` renders every screenshot and six GIF clips offscreen | accepted |
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
| 2026-11-18 | [Quality roadmap](2026-11-18-quality-roadmap.md): phases for tooling, tests, shared presentation, i4sim, Demo, Dotio, predictive passing and documentation cleanup | accepted |
| 2026-11-18 | [Tooling and CI](2026-11-18-tooling-and-ci.md): roadmap phase 2 | implemented |
| 2026-11-18 | [Development process and agent rules](2026-11-18-development-process.md) | implemented |
