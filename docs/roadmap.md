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

Within each section the first item comes first. The order was proposed on 2026-11-21 and settled with the user on 2026-11-22 ([design](design/2026-11-21-roadmap-priorities.md#decisions)); on 2026-11-23 the user put *Leaner agentic cycle* first among the agent's work and *Demo scenario menu* before *README media as GIFs*; the other items of that day stand at the end of their sections ([design](design/2026-11-23-roadmap-media-labs-cycle.md#decisions)). The quality roadmap of 2026-11-18 is closed: its phases 1 to 7 and 9 are merged, and everything it left, phase 8 included, is an item on this page.

## Now

- **Leaner agentic cycle** · workflow, tools, CI, with Kinetra · added 2026-11-23
  The user wants the agents' development cycle to take less time and fewer steps. First step, on branch `ccr-6e6399fb-049f2v`: CI runs only the docs check for a docs-only pull request or a pointer move that Lab's build does not read, and such pull requests need no agent review ([design](design/2026-11-23-leaner-cycle-first-step.md), with the measurement of 18 to 23 November). Candidates for the next measures, besides *Ready-to-build cloud sessions*, *Impact-based verification script* and *CI image with the toolchain built in*: a workflow that opens and merges the pointer-only pull request after a Kinetra merge; one design document for a change across both repositories; no design document for a docs-only change; a CI skip in Kinetra; a check that names the GIFs a diff makes stale; and, as the largest step, Kinetra and Lab in one repository (see *Repositories per lab*).
  Next step: the user chooses among the candidates ([open points](design/2026-11-23-leaner-cycle-first-step.md#open-points)). Where: [workflow](../external/Kinetra/docs/workflow.md); Kinetra item of the same name.

## Next

The user's checks come first: each is short and unblocks the items after it. The agent's work follows, in the order settled on 2026-11-22.

- **Dotio playtest and phase 6's decisions** · dotio · added 2026-11-21
  Phase 6 ends with a playtest by the user and a review of what the agent decided alone: route planning on Kinetra's `WeightedGrid` instead of `Navigation`; Kinetra's new `search`, which picks a shelter on route cost only, so an enemy may choose a slightly different place than before; whole routes without the old 50-cell cap; four random streams from the world seed, with `--seed` on `kinetra_dotio`; and refreshed Dotio captures without a new tour video. Recovery is too rare in the benchmark to compare shelter searches, so the playtest should watch it.
  Next step: the user plays with the [playtest checklist](design/2026-11-21-dotio-restructuring.md#playtest-checklist), then confirms or changes the decisions (agreed on 2026-11-22: after the playtest, not before). Where: [Dotio restructuring, open points](design/2026-11-21-dotio-restructuring.md#open-points).
- **Windows zip on a desktop** · release · added 2026-11-21
  The release workflow is merged; its done-criterion is the user's: download `Kinetra-Lab-windows-x64.zip` from the README link and start the launcher on Windows without installing anything.
  Next step: the user's desktop check. Where: [Windows release zip](design/2026-11-21-windows-release-zip.md).
- **Smooth-wheel zoom on a desktop** · demo, dotio, i4sim, with Kinetra · added 2026-11-22
  Kinetra's runtime now reads `wheel.preciseY` ([Kinetra #30](https://github.com/SMB-M87/Kinetra/pull/30)), so a touchpad or smooth wheel zooms Demo, Dotio and i4sim in fractional steps instead of whole notches; a notched wheel zooms as before. Software rendering cannot judge the feel. Kinetra also asks for one run of its `graphics` workflow under WSL2, to confirm the cause of its old mouse-input failure.
  Next step: the user's playtest with a touchpad and with a notched wheel. Where: [graphics tests in CI, open points](design/2026-11-22-graphics-tests-in-ci.md#open-points).
- **Ready-to-build cloud sessions** · build, tools · added 2026-11-21
  A cloud session starts on Ubuntu 24.04 without SDL2, nlohmann JSON and ccache, and with clang-format 18 where CI uses 23 (checked on 2026-11-21). Every code task therefore installs packages first and cannot reproduce CI's format check. On 2026-11-22 the *Linux beyond Arch* session created a personal cloud environment *Kinetra-Lab* for Kinetra-Lab and Kinetra, whose setup script installs the README's Debian and Ubuntu packages; clang-format 23 is not in Ubuntu's packages and is still missing.
  Next step: start the next code task in the *Kinetra-Lab* environment and check that it builds at once; then decide how a session gets clang-format 23 (for example LLVM's apt repository, which needs its host allowed).
- **i4sim session fixes** · i4sim · added 2026-11-21
  Decided on 2026-11-22 that all four are bugs: `R` keeps a `--ups` rate, RESET PARAMETERS sets the horizon back to the default of 32 instead of 8, the unused `output` argument of `create_session` goes, and a message such as a rejected goal also ages while no blueprint is loaded. One small pull request, a test per point; the session is presentation code, so it ends at the Full level.
  Next step: branch `fix/i4sim-session`. Where: [i4sim open points](../labs/i4sim/README.md#open-points).
- **Remove Demo's help overlay** · demo · added 2026-11-21
  Decided on 2026-11-22: `draw_help`, `toggle_help` and `PresentationOptions::help`, reachable only from tests, go with their tests; the help lines stay in the settings panel, so nothing changes on screen.
  Next step: branch `refactor/demo-help-overlay`. Where: [Demo open points](../labs/demo/README.md#open-points).
- **Performance baseline and hotspots** · i4sim, demo, dotio, with Kinetra · added 2026-11-21
  The user wants a critical look at performance. Measure first: profile `kinetra_i4sim_motion_benchmark` and the Demo crowd cases in the `release` preset, add a Dotio benchmark, and list the hotspots with their share of the run time. Known candidates: predictive passing takes about half again as long on the large i4sim layout (9–10.5 s against 13–14.5 s), Dotio's shelter search can settle the whole resident window of 24,025 cells, and Demo's fixed clock falls behind below 30 frames per second at time scale 4. Each optimization is then its own pull request, with a measurement before and after and the golden lines unchanged.
  Next step: the profile and the hotspot list, without code changes. Where: [motion benchmark](../labs/i4sim/tests/README.md#motion-benchmark), [Dotio open points](../labs/dotio/README.md#open-points), [Demo open points](../labs/demo/README.md#open-points); Kinetra item *Engine performance*.
- **Demo scenario menu** · demo, launcher · added 2026-11-23
  The user wants a page in the launcher on which a Demo scenario is chosen, as Dotio's difficulty is, with its own Demo environment for each choice that showcases one part of the engine. Today Demo is one scene, a player and a crowd in endless seeded terrain, and registers no configurations (`launcher/src/applications.cpp`), although Kinetra's menu gives any application a configuration page (`LaunchChoice::configurations`). Kinetra's GIFs of the shared UI wait for this item, so that the configuration page comes from Demo too (the user's decision of 2026-11-23).
  Next step: a design with the user (`grilling`): which environments, for example the current terrain with its crowd, physics with gravity and contacts, route planning through a maze, and avoidance in a doorway with RECIPROCAL and LEGACY side by side; what `--variant demo` and `--agents` start; and where it ends and the *Motion lab* idea begins, whose crowd scenarios overlap. Where: [Demo](../labs/demo/README.md), [launcher](../launcher/README.md).
- **README media as GIFs** · media, capture, docs, with Kinetra · added 2026-11-23
  The user wants the Lab README to be about the labs themselves, with as many GIFs as it takes to show how each lab works and what it supports, because a clip shows behavior a screenshot cannot; and Kinetra's README to show the shared UI in GIFs captured from Demo: the launcher, the configuration page (Demo's scenarios), the settings panel, the debug menu, the agent inspector, the frame times and the other debug views. With it comes a rule for agents in both repositories: a change that alters what is visible refreshes the affected GIFs in the same pull request, which replaces rendering the tour after larger changes (*Tour video for YouTube*). Today the Lab gallery has eleven clips and three launcher screenshots, Kinetra's README has no media, and Demo has no configuration page yet. Captures are deterministic, so a refresh adds only the GIFs whose pixels changed to the history.
  Next step: a design: the scenes per lab and per shared overlay, where Kinetra's GIFs live (proposal: `docs/media/` in Kinetra, written by this capture tool), a size budget per GIF, and the rule's wording in both `AGENTS.md` files and the review checklist. Depends on: *Demo scenario menu*, for Kinetra's GIFs; the Lab README's GIFs can come first. Where: [media guide](media/README.md), [capture tool](../tools/capture/README.md); Kinetra item *Shared UI in the README*.

## Later

- **Stall detection and remedies in i4sim** · i4sim · added 2026-11-21
  A mover recognizes that it is stalled and why (sealed in by parked neighbors, a standoff with another mover, sliding back and forth), and the simulation answers with optional remedies, off by default and measured with the [48-run harness](design/2026-11-20-i4sim-tuning-stall.md#measurement-harness). Decided on 2026-11-22, as a stack: (1) detection and its events without behavior change, shown in the inspector, measured over more seeds and with passing on as well, so phase 7's 11-against-6 stuck movers turns out a trend or not; (2) MAKE WAY: parked neighbors on the stalled mover's route make way, also a mover that waits for a producer with a product, which keeps its product and its place in the queue and returns when the stalled mover has passed or after a fixed time; (3) a temporary motion fallback whose motion type and return rule the user chooses from the detection data. The documented standstill needs MAKE WAY: no motion type gets out while the neighbors stay parked.
  Open question for the user, phase 7's third: should the departure check refuse a route that steering cannot follow past a parked neighbor? The detection step first explains why it accepted mover 25's route past parked mover 26 (`4320x2400`, seed 2026, tick 7730); the user decides with that cause in hand, before MAKE WAY is built.
  Uses the [agent inspector](design/2026-11-22-agent-inspector-overlay.md). Where: [phase 7, step 4](design/2026-11-21-i4sim-passing-under-pressure.md#step-4-a-documented-limit-and-a-proposed-remedy), [phase 7, open points](design/2026-11-21-i4sim-passing-under-pressure.md#open-points).
- **RECOVERY detour past a corner** · i4sim · added 2026-11-21
  Route recovery can plan a detour that passes a blocked cell's corner with 3 mm to spare, which the border constraint then refuses: mover 18 on `4320x2400` under Improved, in six of the 48 runs with RECOVERY on.
  Where: [phase 7, open points](design/2026-11-21-i4sim-passing-under-pressure.md#open-points).
- **Shared passing mechanism** · i4sim, demo, dotio, with Kinetra · added 2026-11-21
  Formerly phase 8 of the quality roadmap. Move the proven, application-free part of predictive passing (conflict prediction, lane agreement, maneuver guard) into Kinetra; i4sim keeps its factory policy. Run i4sim's pressure scenarios against the Kinetra mechanism, and give Demo crowds and Dotio enemies the same mechanism and scenarios. Kinetra's item also decides on one agent-motion interface for its four steering models.
  Depends on: *Stall detection and remedies in i4sim*. Where: [Kinetra roadmap](../external/Kinetra/docs/roadmap.md#later), *Shared passing mechanism*.
- **Golden tests on Windows** · tests, with Kinetra · added 2026-11-22
  Four Demo crowd characterization cases and i4sim's RVO case still differ on Windows after Kinetra's `atan2` and `asin`, so the Windows presets exclude them; Dotio's fingerprint and the rest hold there. Another function of Windows' C runtime rounds differently, among `sinf`, `cosf`, `expf`, `powf` and `hypot`. Routing it through Kinetra as well would also move the recorded lines on Linux, because glibc's `sinf` and `cosf` are not correctly rounded either, so the lines would be recorded again once.
  Next step: find the function by trying each on Windows CI with a correctly rounded replacement, then the user decides whether to re-record. Where: [Windows tests](windows.md#tests), [Linux beyond Arch, open points](design/2026-11-22-linux-beyond-arch.md#open-points).
- **Linux release** · release · added 2026-11-21
  A download for Linux next to the Windows zip: the launcher and the three applications with `i4sim-blueprints/` and the licenses, built on an older distribution (for example Ubuntu 22.04) so that its C library runs on current ones, first as a tarball with SDL2 inside and OpenGL from the system; an AppImage can follow. The release workflow gets a second job, and the README a second link.
  Unblocked by *Linux beyond Arch* (2026-11-22); it stays here until the user orders it. Where: [release tool](../tools/release/README.md).
- **Frame pacing in WSLg** · runtime, build · added 2026-11-21
  The Linux build stutters in WSLg and the Windows build does not; the design document lists three options and recommends measuring option 1 (Wayland with frame callbacks) first. On 2026-11-22 the user kept the choice open.
  Next step: the user's choice, when they come back to it. Where: [Frame pacing in WSLg](design/2026-11-20-wslg-frame-pacing.md) (draft).
- **CI image with the toolchain built in** · CI, with Kinetra · added 2026-11-22
  CI installs the pinned toolchain from the Arch Linux Archive in every run and falls back to the current packages when the archive does not answer, as on 2026-11-21. An image with the pinned packages installed, on GitHub's container registry, would make CI independent of the archive and faster; it can only be built while the archive answers.
  Next step: Kinetra's item leads: a design of who builds and publishes the image, and how a snapshot bump updates it; Lab's `ci.yml` and `release.yml` then use the same image. Where: Kinetra item of the same name.
- **Impact-based verification script** · tools · added 2026-11-21
  One command that reads the changed paths and runs the level, targets and CTest regex of the table in [verification in Lab](../AGENTS.md#verification-in-lab), with that table in a file both the script and the page use. The `verify` skill and developers then run the same selection, and a forgotten target becomes rare.
  Next step: a design: where the table lives, and how Kinetra's own table joins it.
- **Reproducible Dotio captures** · capture, media · added 2026-11-21
  Dotio draws from its seed since phase 6, and the capture tool starts it with `capture_seed`, yet the capture README and `capture_tour.sh` still say its part differs per run, and its scripts react to the world instead of following a timeline. Check whether two captures are now identical, and update the docs and scripts. The tour is no longer rendered here: on 2026-11-23 the user decided that one more tour video is made, for YouTube (*Tour video for YouTube*).
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
- **Lab preview in the launcher** · launcher, with Kinetra · added 2026-11-23
  While a lab is highlighted in the launcher menu, by the pointer or the arrow keys, an overlay shows a picture or a short clip of it with its description, so a new user sees what each lab is before starting it. The registrations already carry a description (`launcher/src/applications.cpp`: "PHYSICS AND NAVIGATION", "SURVIVAL", "LOCAL FACTORY SIMULATION"), but Kinetra's menu does not draw it, and its runtime draws only colored shapes and bitmap text, so images are Kinetra's part. Lab's part: a description of a few lines per lab, and its preview, made by the capture tool.
  Open questions: a still picture, an animated clip or the live session in miniature; previews built into the executable or read from files next to it, as `i4sim-blueprints/` is.
  Depends on: Kinetra's *Images in the launcher menu*; the lab clips of *README media as GIFs*. Where: [launcher](../launcher/README.md), [Kinetra roadmap](../external/Kinetra/docs/roadmap.md#later).
- **Tour video for YouTube** · media, capture · added 2026-11-23
  When the other items of Next and Later in both roadmaps are done, the user makes one clear and extensive tour video, uploads it to YouTube and puts its link in the README; then `docs/media/kinetra-lab-tour.mp4` leaves the repository, and the tour is no longer rendered after larger changes, because the GIFs show the current state (*README media as GIFs*). A clone holds three renders of the file (61, 89 and 94 MB, checked on 2026-11-23): deleting it stops the growth, but only a rewrite of the history, the user's call, would make clones smaller. So the new video goes to YouTube and is not committed.
  `capture_tour.sh` renders it (the user's choice of 2026-11-23), into a file outside `docs/media`, so the tour tool stays. Open question: whether the thumbnail stays as the link's picture.
  Depends on: the other items of Next and Later in both roadmaps on 2026-11-23; the Ideas do not count (confirmed by the user on 2026-11-23). Where: [media guide](media/README.md), [README](../README.md#screenshots-and-demo).

## Ideas

Wishes of the user, not yet agreed or shaped. Each needs a design session (`grilling`) before it moves up.

- **Motion lab** · new application · added 2026-11-21
  For the future, no priority (the user's words): a fourth lab that shows motion and looks polished, for example crowd control. The agent's proposal: classic scenarios (a bottleneck or doorway evacuation, counter-flow in a corridor, crossing flows, agents that swap places across a circle), the steering models side by side in the same scenario, heat maps of density, speed and stalls, trails, and live measurements (throughput, contacts, time to goal). It would also be the visual test bed for the shared passing mechanism and one agent-motion interface.
  Depends on: *Shared passing mechanism*. Uses the [agent inspector](design/2026-11-22-agent-inspector-overlay.md).
- **Kinetra-Lab on Android** · launcher, release, with Kinetra · added 2026-11-21
  Run the launcher and the applications on an Android phone or tablet. Lab's part: touch-friendly menus and overlays, the blueprints of `i4sim-blueprints/` packaged as assets, and an APK build next to the Windows zip.
  Depends on: Kinetra's *Android runtime* idea (an OpenGL ES path, touch input, an NDK build).
- **3D Skatepark lab** · new application, with Kinetra · added 2026-11-21
  A fourth lab based on the user's [Skatepark](https://github.com/SMB-M87/Skatepark) repository: a skatepark built in C++ with OpenGL, freeglut, GLEW and glm, from procedurally generated cubes, prisms, cylinders, spheres and tori, a skateboard model from Blender, and a ground and an aerial camera.
  Depends on: Kinetra's *3D rendering* idea. Open questions: port Skatepark's geometry and camera, or rebuild them on Kinetra's 3D module; which parts are headless logic that can be tested without a window.
- **Documentation as a site** · docs, with Kinetra · added 2026-11-21
  The user asked whether the docs could move to the GitHub wiki, and whether everything would keep working. A wiki is a separate repository without pull requests or CI: design documents and READMEs could no longer change in the same pull request as the code, the docs check would not run, relative links to code and between the two repositories would break, and agents would no longer find the pages in their checkout. The agent's advice: keep `docs/` as the source, and if a browsable site is wanted, publish a read-only copy of it (on the wiki or GitHub Pages) on every merge.
- **Evolution lab** · new application · added 2026-11-23
  The user's idea: a world in which agents act on their own, perhaps communicate, learn from their behavior and evolve; no language models. The agent's proposal: every agent has a genome (body traits such as size, speed and sensor range) and a small neural network as its brain, senses through rays (food, walls, other agents and their signals) and its own energy, and acts by turning, moving, eating, emitting a signal and reproducing. Evolution needs no fitness function: an agent with enough energy gets a mutated child; a generational mode next to it allows measured experiments. In steps, each a lab on its own: foraging; predators and prey with species; evolved signals such as alarm or food calls, measured by how much a signal says about what its sender sees; learning within a lifetime (plasticity, imitation of neighbors). Headless and seeded like the other labs, so generations run fast without a window, with species colors, population graphs and the agent inspector showing a brain.
  Open questions: one open world or a scenario per experiment; fixed networks or growing ones (NEAT); whether genome and network move to Kinetra once proven, since they hold no game rules; its order next to the *Motion lab*. An identical history across compilers depends on *Golden tests on Windows*.
- **Repositories per lab** · repository layout, with Kinetra · added 2026-11-23
  The user asked whether Demo, Dotio, i4sim and later labs should become repositories of their own, runnable alone or inside Lab, and doubts it. The agent's advice: keep one Lab repository. Each lab repository would carry its own Kinetra submodule, so Lab would hold several pointers that must agree while a build can contain only one Kinetra, and every Kinetra merge would need a pointer pull request per repository instead of one; the shared code in `labs/shared`, the bundled launcher, the capture tool, the release zip and CI serve all labs at once. Every lab already runs alone (`kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim`); a CMake option that builds only one lab would add the rest of standalone use without a split. A split pays off only for a lab with an audience or license of its own. The opposite move, Kinetra and Lab in one repository, would end the pointer pull requests and double design documents, at the cost of Kinetra as a repository of its own (*Leaner agentic cycle*).
