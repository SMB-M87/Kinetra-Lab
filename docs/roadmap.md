# Roadmap

The backlog of Kinetra-Lab: application, tool and build work that is wanted but not finished, from agreed follow-ups to loose ideas. It is the only plan: work in progress is described in its design document, and this page says what comes after it, in order of priority. Engine work lives in the [Kinetra roadmap](../external/Kinetra/docs/roadmap.md).

## Table of contents

- [How to use this page](#how-to-use-this-page)
- [Now](#now)
- [Next](#next)
- [Later](#later)
- [Ideas](#ideas)

[Documentation index](README.md) · [Project overview](../README.md) · [Design documents](design/README.md)

## How to use this page

Sections, item format and the moves between sections are defined in the [documentation guide](../external/Kinetra/docs/documentation.md#roadmap). The `roadmap` skill adds, shapes and closes items, for example during a brainstorm with the user ([skill](../external/Kinetra/.agents/skills/roadmap/README.md)). A merged item is removed; its design document is the record.

Within each section the first item comes first. The order was proposed on 2026-11-21 and is settled with the user ([design](design/2026-11-21-roadmap-priorities.md)). The quality roadmap of 2026-11-18 is closed: its phases 1 to 7 and 9 are merged, and everything it left, phase 8 included, is an item on this page.

## Now

Nothing in progress.

## Next

The user's decisions and checks come first: each is short and unblocks the items after it. The agent's work follows.

- **Decide on making way and a temporary motion fallback** · i4sim · added 2026-11-21
  Phase 7 found that a mover can be sealed in for good by parked neighbors that never make way, and proposed an optional MAKE WAY policy, off by default. The user adds a wish: a mover that recognizes a coming deadlock under its motion settings switches to another motion type for a while, and back once it is free. Both answer one question, what a stalled mover may do, and they shape *Stall detection and remedies in i4sim*.
  Next step: a design session with the user (`grilling`) on phase 7's open questions (may a mover that waits for a producer leave its place; how production treats a mover that makes way; should the departure check refuse a route that steering cannot follow past a parked neighbor) and on the fallback (in which situations, to which motion type, when to switch back). Where: [phase 7, step 4](design/2026-11-21-i4sim-passing-under-pressure.md#step-4-a-documented-limit-and-a-proposed-remedy).
- **Dotio playtest and phase 6's decisions** · dotio · added 2026-11-21
  Phase 6 ends with a playtest by the user and a review of what the agent decided alone: route planning on Kinetra's `WeightedGrid` instead of `Navigation`; Kinetra's new `search`, which picks a shelter on route cost only, so an enemy may choose a slightly different place than before; whole routes without the old 50-cell cap; four random streams from the world seed, with `--seed` on `kinetra_dotio`; the golden fingerprint skipped on Windows; and refreshed Dotio captures without a new tour video. Recovery is too rare in the benchmark to compare shelter searches, so the playtest should watch it.
  Next step: the user plays with the [playtest checklist](design/2026-11-21-dotio-restructuring.md#playtest-checklist) and confirms or changes the decisions. Where: [Dotio restructuring, open points](design/2026-11-21-dotio-restructuring.md#open-points).
- **Phase 7's decisions** · i4sim · added 2026-11-21
  Also decided by the agent alone: three stacked pull requests; Improved's extra contacts documented as a profile limit; the yield order (outside movers, then older plans), ignoring movers behind, ending a plan at once for an oncoming mover, the 20-tick resume and the 240-tick limit before a plan ends as `obstructed`; and passing still off by default, although Reciprocal gains in every measured run. With seed 42 on `8640x4800` under Reciprocal the guard leaves more movers stuck at some point (11) than no passing does (6), with better throughput and waiting; more seeds would show whether that is a trend.
  Next step: the user confirms or changes them. Where: [phase 7, open points](design/2026-11-21-i4sim-passing-under-pressure.md#open-points).
- **Windows zip on a desktop** · release · added 2026-11-21
  The release workflow is merged; its done-criterion is the user's: download `Kinetra-Lab-windows-x64.zip` from the README link and start the launcher on Windows without installing anything.
  Next step: the user's desktop check. Where: [Windows release zip](design/2026-11-21-windows-release-zip.md).
- **Frame pacing in WSLg** · runtime, build · added 2026-11-21
  The Linux build stutters in WSLg and the Windows build does not; the design document lists three options.
  Next step: the user chooses one. Where: [Frame pacing in WSLg](design/2026-11-20-wslg-frame-pacing.md) (draft).
- **i4sim session behavior** · i4sim · added 2026-11-21
  `R` drops a `--ups` rate, RESET PARAMETERS sets a horizon of 8 against the default 32, `create_session` ignores its `output` argument, and a message such as a rejected goal stays on the blueprint screen when the blueprint is closed within its four seconds.
  Next step: the user decides the intended behavior. Where: [i4sim open points](../labs/i4sim/README.md#open-points).
- **Demo's help overlay** · demo · added 2026-11-21
  `draw_help` and `toggle_help` are reachable only from tests.
  Next step: the user decides: remove them, or draw the legend. Where: [Demo open points](../labs/demo/README.md#open-points).
- **Pull request review skill** · skills, with Kinetra · added 2026-11-21
  The user wants an agent to review every pull request: inline comments and a summary with a conclusion and points of attention, never an approval or a merge. The agent that opened the pull request picks the comments up with `pr-feedback` and merges them up its stack, so the user finds a reviewed pull request whose conflicts are already resolved. The skill lives in Kinetra, like the others.
  Next step: the design session of Kinetra's item. Where: [Kinetra roadmap](../external/Kinetra/docs/roadmap.md#next), *Pull request review skill*.
- **Ready-to-build cloud sessions** · build, tools · added 2026-11-21
  A cloud session starts on Ubuntu 24.04 without SDL2, nlohmann JSON and ccache, and with clang-format 18 where CI uses 23 (checked on 2026-11-21). Every code task therefore installs packages first and cannot reproduce CI's format check. A setup script in the cloud environment would install them once per container.
  Next step: the agent proposes the script in the first session that builds code; the user approves it in the environment's settings prompt.
- **Golden tests on every toolchain** · tests · added 2026-11-21
  Four Demo crowd characterization cases and i4sim's RVO golden case hold only for CI's pinned Arch toolchain. They fail with Ubuntu's GCC 13, the compiler of cloud sessions, and are skipped on Windows; so the local Full level fails there although nothing changed, and *Linux beyond Arch* cannot start. Choose between a tolerance, observable totals instead of trajectory fingerprints, and a fingerprint per toolchain.
  Next step: record which values differ on GCC 13, and by how much. Where: [Demo restructuring, open points](design/2026-11-21-demo-restructuring.md#open-points), [Dotio restructuring, open points](design/2026-11-21-dotio-restructuring.md#open-points).
- **Graphics tests in CI** · tests, CI, with Kinetra · added 2026-11-21
  CI compiles Lab's graphics tests and smoke runs but does not run them. The `graphics` preset already renders with SDL's offscreen driver and software Mesa, as the capture tool does, and needs no GPU. Running it in CI checks presentation, launcher and overlay changes on every pull request: the inspector overlay below needs that, and so do agents in cloud sessions without a display.
  Next step: run `cmake --workflow --preset graphics` in CI's Arch image, together with Kinetra's item of the same name.
- **Agent inspector overlay** · i4sim, demo, dotio, with Kinetra · added 2026-11-21
  Click an agent to see its details in an overlay: motion profile and parameters, goal and route, state, speed and forces, and in i4sim its passing plan and waiting time. The rows are headless logic, tested without a window like the debug menu's rows, and a command-line option can print the same rows per tick, so a test or an agent sees what the user sees. i4sim and Demo already select an agent (`FactoryController::selected_mover`, `Scene::selected_agent`); Dotio does not yet.
  Next step: a design session; then a stack of Kinetra's overlay, i4sim, Demo and Dotio. Where: Kinetra item of the same name.
- **Performance baseline and hotspots** · i4sim, demo, dotio, with Kinetra · added 2026-11-21
  The user wants a critical look at performance. Measure first: profile `kinetra_i4sim_motion_benchmark` and the Demo crowd cases in the `release` preset, add a Dotio benchmark, and list the hotspots with their share of the run time. Known candidates: predictive passing takes about half again as long on the large i4sim layout (9–10.5 s against 13–14.5 s), Dotio's shelter search can settle the whole resident window of 24,025 cells, and Demo's fixed clock falls behind below 30 frames per second at time scale 4. Each optimization is then its own pull request, with a measurement before and after and the golden lines unchanged.
  Next step: the profile and the hotspot list, without code changes. Where: [motion benchmark](../labs/i4sim/tests/README.md#motion-benchmark), [Dotio open points](../labs/dotio/README.md#open-points), [Demo open points](../labs/demo/README.md#open-points); Kinetra item *Engine performance*.

## Later

- **Stall detection and remedies in i4sim** · i4sim · added 2026-11-21
  A mover recognizes that it is stalled and why (sealed in by parked neighbors, a standoff with another mover, sliding back and forth), and the simulation answers with the remedies chosen in the decision above: neighbors make way, or the mover drives with another motion type for a while and returns to its setting once it moves again. The documented standstill needs making way: while its neighbors stay parked there is no way out, whatever the motion type. A motion fallback fits standoffs and jostling. The remedies are optional policies, off by default, measured with the [48-run harness](design/2026-11-20-i4sim-tuning-stall.md#measurement-harness) and the benchmark; the inspector shows the stall, its cause and the active remedy. Proposed stack: detection and its events without behavior change, making way, the motion fallback.
  Depends on: *Decide on making way and a temporary motion fallback*, *Agent inspector overlay*. Where: [phase 7, step 4](design/2026-11-21-i4sim-passing-under-pressure.md#step-4-why-two-movers-stand-still-near-a-destination).
- **RECOVERY detour past a corner** · i4sim · added 2026-11-21
  Route recovery can plan a detour that passes a blocked cell's corner with 3 mm to spare, which the border constraint then refuses: mover 18 on `4320x2400` under Improved, in six of the 48 runs with RECOVERY on.
  Where: [phase 7, open points](design/2026-11-21-i4sim-passing-under-pressure.md#open-points).
- **Shared passing mechanism** · i4sim, demo, dotio, with Kinetra · added 2026-11-21
  Formerly phase 8 of the quality roadmap. Move the proven, application-free part of predictive passing (conflict prediction, lane agreement, maneuver guard) into Kinetra; i4sim keeps its factory policy. Run i4sim's pressure scenarios against the Kinetra mechanism, and give Demo crowds and Dotio enemies the same mechanism and scenarios. Kinetra's item also decides on one agent-motion interface for its four steering models.
  Depends on: *Stall detection and remedies in i4sim*. Where: [Kinetra roadmap](../external/Kinetra/docs/roadmap.md#later), *Shared passing mechanism*.
- **Linux beyond Arch** · build, docs, CI, with Kinetra · added 2026-11-21
  The README gives `pacman` for Artix and Arch and one Debian package name, and CI runs only the pinned Arch image. Support Linux in general and keep Artix and Arch working: dependency commands per distribution family (Arch and Artix, Debian and Ubuntu, Fedora), a CI job on Ubuntu LTS next to the pinned Arch job, which stays the reference for warnings and formatting, and docs that name what is tested.
  Depends on: *Golden tests on every toolchain*; Kinetra's item of the same name. Where: [README, quick start](../README.md#quick-start).
- **Linux release** · release · added 2026-11-21
  A download for Linux next to the Windows zip: the launcher and the three applications with `i4sim-blueprints/` and the licenses, built on an older distribution (for example Ubuntu 22.04) so that its C library runs on current ones, as a tarball or an AppImage with SDL2 inside and OpenGL from the system. The release workflow gets a second job, and the README a second link.
  Depends on: *Linux beyond Arch*. Where: [release tool](../tools/release/README.md).
- **Impact-based verification script** · tools · added 2026-11-21
  One command that reads the changed paths and runs the level, targets and CTest regex of the table in [verification in Lab](../AGENTS.md#verification-in-lab), with that table in a file both the script and the page use. The `verify` skill and developers then run the same selection, and a forgotten target becomes rare.
  Next step: a design: where the table lives, and how Kinetra's own table joins it.
- **Reproducible Dotio captures and a new tour video** · capture, media · added 2026-11-21
  Dotio draws from its seed since phase 6, and the capture tool starts it with `capture_seed`, yet the capture README and `capture_tour.sh` still say its part differs per run, and its scripts react to the world instead of following a timeline. The tour video was not rendered again after phases 5 to 7. Check whether two captures are now identical, update the docs and scripts, and render the tour.
  Where: [capture tool open points](../tools/capture/README.md#open-points).
- **Dotio on `kinetra::Vec2`** · dotio · added 2026-11-21
  `dotio::math::Vec2` duplicates Kinetra's vector with extra operators and seven aliases.
  Where: [Dotio open points](../labs/dotio/README.md#open-points).
- **Smoother pursuit in Dotio** · dotio, with Kinetra · added 2026-11-21
  Route following passes waypoints by distance and does not skip those already in sight, so pursuers cut fewer corners than they could.
  Depends on: Kinetra's *Raster route target*. Where: [Dotio open points](../labs/dotio/README.md#open-points).
- **Timing header out of `runtime/`** · demo, dotio, i4sim, with Kinetra · added 2026-11-21
  The logic of all three applications includes `<kinetra/runtime/timing.h>`, which is headless; their includes follow when Kinetra moves it.
  Depends on: Kinetra's *Headless timing outside `runtime/`*.
- **Capture on Kinetra's frame read-back** · capture, tests, with Kinetra · added 2026-11-21
  The capture tool and the graphics tests call `glReadPixels` themselves.
  Depends on: Kinetra's *Frame read-back on `Runtime`*. Where: [capture tool open points](../tools/capture/README.md#open-points).
- **Menu rows by label in tests and scripts** · i4sim, capture, tests · added 2026-11-21
  The i4sim graphics suite and the clip and screenshot scripts click debug-menu rows by number, while the tour finds them by label (`kinetra_lab::Session::debug_rows()`). Labels remove numbers that are kept up to date by hand. Related: with LAYERS open the i4sim debug menu is 24 rows high, so in a window lower than about 650 pixels FRAME SCENE and BLUEPRINTS fall below the edge.
  Where: [i4sim open points](../labs/i4sim/README.md#open-points), [capture tool open points](../tools/capture/README.md#open-points).
- **One i4sim session frame helper** · i4sim, capture · added 2026-11-21
  The two i4sim measurements repeat the session's frame order by hand; a helper in the i4sim logic would serve both.
  Where: [capture tool open points](../tools/capture/README.md#open-points).
- **Launcher `--seed`** · launcher · added 2026-11-21
  The bundled launcher cannot replay a run exactly; `kinetra_i4sim` and `kinetra_dotio` take a seed.
  Where: [launcher open points](../launcher/README.md#open-points).
- **Warnings as errors for the launcher and shared code** · launcher, shared, build · added 2026-11-21
  Demo, i4sim, Dotio and the capture tool turn Lab warnings into errors; `launcher/` and `labs/shared/` only report them.
  Next step: build with `dev`, fix what they report, and add both to the `AS_ERRORS` lines of the root `CMakeLists.txt` ([rule](../AGENTS.md#verification-in-lab)).
- **More checks in CI** · tests, CI · added 2026-11-21
  The `asan` preset (AddressSanitizer and UBSan) and `.clang-tidy` exist, but CI runs neither. A sanitizer job, clang-tidy on the changed files and a coverage report of the logic suites would find memory errors, undefined behavior and untested code before review.
- **Blueprint loader fuzzing** · i4sim, tests · added 2026-11-21
  Blueprints are files, so the JSON loader is an input boundary. A fuzz target on the loader (libFuzzer with clang) would check that every invalid file ends in an error, never in a crash.

## Ideas

Wishes of the user, not yet agreed or shaped. Each needs a design session (`grilling`) before it moves up.

- **Motion lab** · new application · added 2026-11-21
  For the future, no priority (the user's words): a fourth lab that shows motion and looks polished, for example crowd control. The agent's proposal: classic scenarios (a bottleneck or doorway evacuation, counter-flow in a corridor, crossing flows, agents that swap places across a circle), the steering models side by side in the same scenario, heat maps of density, speed and stalls, trails, and live measurements (throughput, contacts, time to goal). It would also be the visual test bed for the shared passing mechanism and one agent-motion interface.
  Depends on: *Shared passing mechanism*, *Agent inspector overlay*.
- **Kinetra-Lab on Android** · launcher, release, with Kinetra · added 2026-11-21
  Run the launcher and the applications on an Android phone or tablet. Lab's part: touch-friendly menus and overlays, the blueprints of `i4sim-blueprints/` packaged as assets, and an APK build next to the Windows zip.
  Depends on: Kinetra's *Android runtime* idea (an OpenGL ES path, touch input, an NDK build).
- **3D Skatepark lab** · new application, with Kinetra · added 2026-11-21
  A fourth lab based on the user's [Skatepark](https://github.com/SMB-M87/Skatepark) repository: a skatepark built in C++ with OpenGL, freeglut, GLEW and glm, from procedurally generated cubes, prisms, cylinders, spheres and tori, a skateboard model from Blender, and a ground and an aerial camera.
  Depends on: Kinetra's *3D rendering* idea. Open questions: port Skatepark's geometry and camera, or rebuild them on Kinetra's 3D module; which parts are headless logic that can be tested without a window.
- **Documentation as a site** · docs, with Kinetra · added 2026-11-21
  The user asked whether the docs could move to the GitHub wiki, and whether everything would keep working. A wiki is a separate repository without pull requests or CI: design documents and READMEs could no longer change in the same pull request as the code, the docs check would not run, relative links to code and between the two repositories would break, and agents would no longer find the pages in their checkout. The agent's advice: keep `docs/` as the source, and if a browsable site is wanted, publish a read-only copy of it (on the wiki or GitHub Pages) on every merge.
