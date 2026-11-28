# Lab design documents

One design document per Lab branch, written before the code and kept after merging, except for a docs-only change. The format, lifecycle and template are shared with Kinetra; see [Kinetra design documents](../../external/Kinetra/docs/design/README.md#template). A change that touches both repositories has one document, in the repository with most of the change, which the other pull request links. The index below is for a search by topic: agents read an older design document only when something links it ([rule](../../external/Kinetra/AGENTS.md#development-cycle)).

## Table of contents

- [Index](#index)

[Parent guide](../README.md) · [Project overview](../../README.md) · [Documentation index](../README.md) · [Workflow](../../external/Kinetra/docs/workflow.md)

## Index

| Date | Document | Status |
| --- | --- | --- |
| 2026-11-27 | [i4sim surroundings](2026-11-27-i4sim-surroundings.md): hotspot 8, the neighbor and border lists of every mover without allocations or a scan of every border | implemented |
| 2026-11-27 | [HUD error style](2026-11-27-hud-error-style.md): Kinetra's shared HUD style, an error in red above the statistics, and an error panel over the launch menu; i4sim uses both | implemented |
| 2026-11-27 | [Demo showcases](2026-11-27-demo-showcases.md): Demo's authored scenarios repeat on their own; Doorway shuttles, Maze becomes a labyrinth with agents, and seven new scenarios show corridors, crossings, a circle, a bottleneck, chaos, fleets and a swarm | implemented |
| 2026-11-27 | [i4sim start state](2026-11-27-i4sim-start-state.md): showcase scenes start running; a blueprint that starts paused shows a RUN button until it first runs | implemented |
| 2026-11-27 | [Default debug layers](2026-11-27-default-debug-layers.md): Demo starts with VECTORS and PATHS, i4sim with every layer but HEATMAP and CONTACT SHAPES | implemented |
| 2026-11-27 | [Debug layers under bodies](2026-11-27-paths-under-bodies.md): the routes, probes, vectors, forces and detection ranges of Demo, Dotio and i4sim are drawn under the moving bodies, with the start of a body's own route or arrow over it; one route width; i4sim's `MAS-demonstrator` fixture renamed `test-environment` | implemented |
| 2026-11-27 | [Tick rates](2026-11-27-tick-rates.md): typed UPS and FPS rows in every lab, up to a million ticks per second, a budgeted clock in Kinetra, and runs of a million ticks that report ticks per second | implemented |
| 2026-11-27 | [Launcher preview stills of their own](2026-11-27-launcher-preview-stills.md): Demo's still shows Doorway as its groups cross, i4sim's the small floor `1680x960+` whole, from capture scenes made for the still | implemented |
| 2026-11-27 | [RECOVERY detour past a corner](2026-11-27-i4sim-recovery-corner.md): mover 18 stuck 3 mm beside a cell; Kinetra's `constrain_borders` gives its face choice a time horizon | implemented |
| 2026-11-27 | [Vec2 equality in place of component comparisons](2026-11-27-vec2-component-comparisons.md) | implemented |
| 2026-11-27 | [Stall remedies in the engine](2026-11-27-stall-remedies-in-the-engine.md): a checked departure from the head of a producer queue, a right-of-way fallback for pushed-back movers, i4sim on Kinetra's `PassingCoordinator`, and stall detection and MAKE WAY in the engine, in five steps | implemented |
| 2026-11-27 | [Configuration button in the settings panel](2026-11-27-settings-configuration-button.md): SCENARIOS, DIFFICULTY and BLUEPRINTS move from the debug menu to the settings panel in place of LAUNCHER, and the pages they open get a LAUNCHER button | implemented |
| 2026-11-27 | [i4sim make-way showcase scene](2026-11-27-i4sim-make-way-showcase.md): the parked-blocker scene of MAKE WAY as the sixth showcase scene, with its clip | implemented |
| 2026-11-27 | [Shared passing mechanism](2026-11-27-shared-passing-mechanism.md): Kinetra's `PassingCoordinator` runs head-on passing for Demo's agents and Dotio's enemies, off by default, with pressure scenarios per application | implemented |
| 2026-11-27 | [i4sim make way](2026-11-27-i4sim-make-way.md) | implemented |
| 2026-11-27 | [Author review in the feature cycle](2026-11-27-inline-feature-review.md): Lab adopts scenario-based verification and the author’s final review | implemented |
| 2026-11-26 | [i4sim stall detection](2026-11-26-i4sim-stall-detection.md) | implemented |
| 2026-11-24 | [Ubuntu as the Linux CI reference](2026-11-24-ubuntu-ci.md) | implemented |
| 2026-11-24 | [Drop the i4sim loadscreen clip](2026-11-24-drop-i4sim-loadscreen-clip.md): the root README's loadscreen GIF and its sentence go, and with them the clip's scene, script, GIF and every page that names it | implemented |
| 2026-11-24 | [Kinetra's .claude/ does not build Lab](2026-11-24-kinetra-claude-unread.md): a pointer move whose Kinetra range adds only `.claude/` runs only the docs check | implemented |
| 2026-11-24 | [The labs in GIFs, and back to the configuration page](2026-11-24-lab-readme-gifs.md): a README section per lab with its GIFs, the launcher stills out, and SCENARIOS and DIFFICULTY rows that return to the launcher's configuration page through Kinetra's new `SessionRequest::configurations` | implemented |
| 2026-11-24 | [Dotio on `kinetra::Vec2`](2026-11-24-dotio-on-kinetra-vec2.md): Dotio uses Kinetra's vector, segment and color types, and Kinetra's vector gets `==`; `dotio::math::Vec2`, its seven aliases and its unused operations go, with the golden fingerprint unchanged | implemented |
| 2026-11-24 | [i4sim contact resolution without every pair](2026-11-24-i4sim-contact-pairs.md): `MotionStep::resolve_contacts` computes each mover's bounds once instead of for every pair, which halves the motion benchmark with every result unchanged; a spatial-index broad phase measured slower at this fleet size | implemented |
| 2026-11-23 | [Ready-to-build cloud sessions](2026-11-23-ready-to-build-cloud-sessions.md): a SessionStart hook in Kinetra and Kinetra-Lab runs one setup script, which loads the submodule, installs the Ubuntu packages and CI's clang-format 23.1.5 at the start of every cloud session | implemented |
| 2026-11-23 | [The shared UI in GIFs in Kinetra's README](2026-11-23-shared-ui-gifs.md): five GIFs of the launcher and overlays from Demo, with a pointer, a fixed frame time in Kinetra's runtime, and a rule to refresh the captures a change makes stale | implemented |
| 2026-11-23 | [The tour's start check names what runs](2026-11-23-tour-start-check.md): `require_demo_terrain` stops the tour with its own message in the menu, in another application and in another Demo scenario | implemented |
| 2026-11-23 | [Demo scenario menu](2026-11-23-demo-scenario-menu.md) | implemented |
| 2026-11-23 | [Demo help overlay removal](2026-11-23-demo-help-overlay-removal.md) | implemented |
| 2026-11-23 | [i4sim session fixes](2026-11-23-i4sim-session-fixes.md): restart rate, motion defaults, blueprint-screen messages and session factory signature | implemented |
| 2026-11-23 | [Shift+right-click inspects in i4sim without holding](2026-11-23-i4sim-shift-right-click.md): Dotio's inspect gesture selects a mover or producer in i4sim and holds or blocks nothing | implemented |
| 2026-11-23 | [Code nits from the last review rounds](2026-11-23-review-nits.md): where the docs check runs, the *No decision script* warning, and the tour's output folder before the render, with Kinetra#39 | implemented |
| 2026-11-23 | [Leaner agentic cycle: the measurement and its first step](2026-11-23-leaner-cycle-first-step.md): CI runs only the docs check when no build, test or zip can change, with the release workflow's `needs_build.sh`; Lab #24 to #81, all of Kinetra's pull requests and the agents' reading measured; the tour video leaves the repository | implemented |
| 2026-11-23 | [Copies check in Lab](2026-11-23-copies-check-in-lab.md): Lab's verification section names Kinetra's `check_copies.py` | implemented |
| 2026-11-23 | [Roadmap: GIFs, a Demo scenario menu, a launcher preview, an evolution lab and a leaner cycle](2026-11-23-roadmap-media-labs-cycle.md): the user's wishes of 2026-11-23 as items in both roadmaps | implemented |
| 2026-11-22 | [Linux beyond Arch and golden tests on every toolchain](2026-11-22-linux-beyond-arch.md): Kinetra's `atan2` and `asin` in the simulation logic, fingerprints on Windows again, an Ubuntu LTS job and dependencies per distribution | implemented |
| 2026-11-22 | [Dotio agent inspector](2026-11-22-dotio-agent-inspector.md) | implemented |
| 2026-11-22 | [Demo agent inspector](2026-11-22-demo-agent-inspector.md) | implemented |
| 2026-11-22 | [Agent inspector overlay](2026-11-22-agent-inspector-overlay.md) | implemented |
| 2026-11-22 | [Graphics tests in CI](2026-11-22-graphics-tests-in-ci.md): the `ci` preset runs the graphics cases and a smoke run of every executable with software OpenGL; CI becomes the whole Full level | implemented |
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
