# Roadmap

The backlog of Kinetra-Lab: application, tool and build work that is wanted but not finished, from agreed follow-ups to loose ideas. It is the only plan: work in progress is described in its design document, or for a docs-only change in its pull request, and this page says what comes after it, in order of priority. Engine work lives in the [Kinetra roadmap](../external/Kinetra/docs/roadmap.md).

## Table of contents

- [How to use this page](#how-to-use-this-page)
- [Now](#now)
- [Next](#next)
- [Later](#later)
- [Ideas](#ideas)

[Documentation index](README.md) · [Project overview](../README.md) · [Design documents](design/README.md)

## How to use this page

Sections, item format and the moves between sections are defined in the [documentation guide](../external/Kinetra/docs/documentation.md#roadmap). The `roadmap` skill adds, shapes and closes items, for example during a brainstorm with the user ([skill](../external/Kinetra/.agents/skills/roadmap/README.md)). A merged item is removed; its design document, or for a docs-only change its pull request, is the record.

Within each section the first item comes first. The order was proposed on 2026-11-21 and settled with the user on 2026-11-22 ([design](design/2026-11-21-roadmap-priorities.md#decisions)); on 2026-11-23 the user put *Leaner agentic cycle* first among the agent's work and *Demo scenario menu* before *README media as GIFs*; the other items of that day stand at the end of their sections ([design](design/2026-11-23-roadmap-media-labs-cycle.md#decisions)). The quality roadmap of 2026-11-18 is closed: its phases 1 to 7 and 9 are merged, and everything it left, phase 8 included, is an item on this page.

## Now

- **Stall detection and remedies in i4sim** · i4sim, with Kinetra · added 2026-11-21
  The departure check, a right-of-way fallback for pushed-back movers, i4sim on Kinetra's `PassingCoordinator`, and stall detection and MAKE WAY in the engine, in a stack of five steps; with Kinetra's *Stall detection and remedies in the engine*.
  Where: [design](design/2026-11-27-stall-remedies-in-the-engine.md), branches `feat/i4sim-departure-check` (step 1), `refactor/i4sim-passing-coordinator` (step 2), `feat/stall-detector` (step 3) and `feat/right-of-way` (step 4).

## Next

- **i4sim toward a million ticks per second** · i4sim, with Kinetra · added 2026-11-27
  The user's goal: a headless run of a million ticks per second. After hotspots 7 and 8 a long run reaches about 300,000 ticks per second on `MAS-demonstrator` and 11,100 on `8640x4800` ([performance](performance.md#measured-changes), [tick rates](design/2026-11-27-tick-rates.md#step-2-the-hotspot-of-a-long-run)). In the profile after hotspot 7, `kinetra::steer` (16%), `resolve_contacts` over every pair (13%), `Production::update` (11%) and `kinetra::validate` (8%) are the next candidates, and under every profile but Original `priority_steer` computes a predictive force that only the FORCES layer and the inspector show; results may shift slightly when the benchmark's metrics do not get worse (the user's decision of 2026-11-27).
  Next step: profile again on top of hotspot 7 and take the largest, one pull request per hotspot, measured before and after. Open question: whether parallel ticks (several simulations, or movers in parallel within one tick) belong on this path.

## Later

- **MAS stops producing in a long run** · i4sim · added 2026-11-27
  In the motion benchmark's first million-tick runs, `MAS-demonstrator` with seed 915839214 completed 29 products, against 202 with seed 42, and ended with a stuck mover: the run stalls early and stays stalled. 50,000 ticks did not show it. After Kinetra's border-face fix of 2026-11-27 both seeds stall: 71 and 93 products, each with a mover stuck at the end.
  Next step: replay the run with `--events` and find the tick where production stops. Where: [tick rates, step 1](design/2026-11-27-tick-rates.md#verification).
- **Golden tests on Windows** · tests, with Kinetra · added 2026-11-22
  Four Demo crowd characterization cases and i4sim's RVO case still differ on Windows after Kinetra's `atan2` and `asin`, so the Windows presets exclude them; Dotio's fingerprint and the rest hold there. Another function of Windows' C runtime rounds differently, among `sinf`, `cosf`, `expf`, `powf` and `hypot`. Routing it through Kinetra as well would also move the recorded lines on Linux, because glibc's `sinf` and `cosf` are not correctly rounded either, so the lines would be recorded again once.
  Next step: find the function by trying each on Windows CI with a correctly rounded replacement, then the user decides whether to re-record. Where: [Windows tests](windows.md#tests), [Linux beyond Arch, open points](design/2026-11-22-linux-beyond-arch.md#open-points).
- **Frame pacing in WSLg** · runtime, build · added 2026-11-21
  The Linux build stutters in WSLg and the Windows build does not; the design document lists three options and recommends measuring option 1 (Wayland with frame callbacks) first. On 2026-11-22 the user kept the choice open.
  Next step: the user's choice, when they come back to it. Where: [Frame pacing in WSLg](design/2026-11-20-wslg-frame-pacing.md) (draft).
- **Impact-based verification script** · tools · added 2026-11-21
  One command that reads the changed paths and runs the level, targets and CTest regex of the table in [verification in Lab](../AGENTS.md#verification-in-lab), with that table in a file both the script and the page use. The `verify` skill and developers then run the same selection, and a forgotten target becomes rare.
  Next step: a design: where the table lives, and how Kinetra's own table joins it.
- **Reproducible Dotio captures** · capture, media · added 2026-11-21
  Dotio draws from its seed since phase 6, and the capture tool starts it with `capture_seed`, yet the capture README and `capture_tour.sh` still say its part differs per run, and its scripts react to the world instead of following a timeline. Check whether two captures are now identical, and update the docs and scripts. The tour is no longer rendered here: on 2026-11-23 the user decided that one more tour video is made, for YouTube (*Tour video for YouTube*).
  Where: [capture tool open points](../tools/capture/README.md#open-points).
- **Smoother pursuit in Dotio** · dotio, with Kinetra · added 2026-11-21
  Route following passes waypoints by distance and does not skip those already in sight, so pursuers cut fewer corners than they could.
  Depends on: Kinetra's *Raster route target*. Where: [Dotio open points](../labs/dotio/README.md#open-points).
- **Menu rows by label in tests and scripts** · i4sim, capture, tests · added 2026-11-21
  The i4sim graphics suite and the clip and screenshot scripts click debug-menu rows by number, while the tour finds them by label (`kinetra_lab::Session::debug_rows()`). Labels remove numbers that are kept up to date by hand. Related: with LAYERS open the i4sim debug menu is 24 rows high, so in a window lower than about 650 pixels FRAME SCENE falls below the edge.
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
- **Dotio shelter search without spikes** · dotio · added 2026-11-23
  A shelter search that finds nothing settles up to the whole window, 7.5 ms when measured on its own, and a recovering enemy near the player repeats it at every decision, 20 times a second. Bound its cost, or search again only when the player or the enemy changes cell. The behavior benchmark gets a time column and a cornered-enemy case: the Dotio benchmark that the baseline item asked for.
  Where: [performance, Dotio](performance.md#dotio), hotspot 6; [Dotio open points](../labs/dotio/README.md#open-points).
- **Tour video for YouTube** · media, capture · added 2026-11-23
  When the other items of Next and Later in both roadmaps are done, the user makes one clear and extensive tour video, uploads it to YouTube and puts its link in the README; the tour is no longer rendered after larger changes, because the GIFs show the current state (*README media as GIFs*, [design](design/2026-11-24-lab-readme-gifs.md)). `docs/media/kinetra-lab-tour.mp4` left the repository on 2026-11-23 at the user's request (*Leaner agentic cycle*): `capture_tour.sh` now renders into `.docs/`, and `.gitignore` keeps `.docs/` and `*.mp4` out. A clone still holds three renders in its history (61, 89 and 94 MB of a 265 MB clone, checked on 2026-11-23); only a rewrite of the history, which the user would run, makes clones smaller ([design, open points](design/2026-11-23-leaner-cycle-first-step.md#open-points)). Open question: whether the thumbnail stays as the link's picture.
  Depends on: the other items of Next and Later in both roadmaps on 2026-11-23; the Ideas do not count (confirmed by the user on 2026-11-23). Where: [media guide](media/README.md), [README](../README.md#the-labs).
- **Cloud setup edge cases** · build, tools · added 2026-11-23
  Three small gaps the agent review left as optional in the cloud setup scripts, two in Kinetra's `tools/cloud_setup.sh` and one in Lab's: a third-party apt source whose update exits 100 still stops the install, `dpkg --configure -a`'s failure is reported as an apt failure, and Lab's message for an old Kinetra commit points to a page that does not name the clang-format version. Like *Ready-to-build cloud sessions*, one item here covers both scripts.
  Where: [Ready-to-build cloud sessions, open points](design/2026-11-23-ready-to-build-cloud-sessions.md#open-points).

- **Showcase drawing and tighter formations in Demo** · demo · added 2026-11-27
  Demo's showcase scripts are not drawn: no debug layer shows a shuttle's stops, a lure or the formation slots. Fleet's formations are loose, because each agent is sent to its slot only every quarter second and slows down within a unit of it; steering by the slot's velocity would hold them tighter.
  Where: [Demo showcases, open points](design/2026-11-27-demo-showcases.md#open-points), [Demo open points](../labs/demo/README.md#open-points).

## Ideas

Wishes of the user, not yet agreed or shaped. Each needs a design session (`grilling`) before it moves up.

- **Motion lab** · new application · added 2026-11-21
  For the future, no priority (the user's words): a fourth lab that shows motion and looks polished, for example crowd control. The agent's proposal: classic scenarios (a bottleneck or doorway evacuation, counter-flow in a corridor, crossing flows, agents that swap places across a circle), the steering models side by side in the same scenario, heat maps of density, speed and stalls, trails, and live measurements (throughput, contacts, time to goal). It would also show how shared passing targets and arrival intent feed each of Kinetra’s four steering solvers.
  Uses Kinetra's `PassingCoordinator` ([shared passing](design/2026-11-27-shared-passing-mechanism.md)) and the [agent inspector](design/2026-11-22-agent-inspector-overlay.md). Since 2026-11-27 Demo holds the classic scenarios as showcases that repeat on their own: Doorway, Corridor, Crossing, Circle and Bottleneck, plus Chaos, Fleet and Swarm ([Demo showcases](design/2026-11-27-demo-showcases.md)); what remains for a lab of its own is the side-by-side comparison, the heat maps, trails and live measurements.
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
  The user asked whether Demo, Dotio, i4sim and later labs should become repositories of their own, runnable alone or inside Lab, and doubts it. The agent's advice: keep one Lab repository. Each lab repository would carry its own Kinetra submodule, so Lab would hold several pointers that must agree while a build can contain only one Kinetra, and every Kinetra merge would need a pointer pull request per repository instead of one; the shared code in `labs/shared`, the bundled launcher, the capture tool, the release zip and CI serve all labs at once. Every lab already runs alone (`kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim`); a CMake option that builds only one lab would add the rest of standalone use without a split. A split pays off only for a lab with an audience or license of its own. The opposite move, Kinetra and Lab in one repository, the user declined on 2026-11-23: Kinetra stays a repository of its own (*Leaner agentic cycle*).
- **i4sim over MCP** · i4sim, tools · added 2026-11-23
  The user asked what MCP (Model Context Protocol) is and whether it helps Lab. The agent's advice: not for building, testing or capturing, which the shell and the scripts already serve, but for a running simulation. An MCP server around the headless i4sim logic would let an agent, or the user in Claude Desktop, examine one run step by step: load a blueprint with a seed, advance a number of ticks, read a mover's route, speed, forces and neighbors, switch a setting such as RECOVERY, and repeat seeded runs. Today each such question needs a throwaway test; the [measurement harness](design/2026-11-20-i4sim-tuning-stall.md#measurement-harness) is a scratch case pasted into `simulation_tests.cpp` by hand. `FactoryController` (`labs/i4sim/include/i4sim/controller.h`) and the simulation's inspector and events (`labs/i4sim/include/i4sim/simulation/`) already work without a window. A candidate use is replaying a documented stall, such as [mover 25 receiving its obstructed departure route](design/2026-11-26-i4sim-stall-detection.md#problem) on `4320x2400` at seed 2026, tick 7730.
  Open questions: a C++ executable under `tools/` that speaks JSON-RPC over stdio with the nlohmann JSON i4sim already uses, or a thin Python server over a headless command line; which tools it offers and what they return; how a cloud session builds and starts it (setup script, `.mcp.json`).
