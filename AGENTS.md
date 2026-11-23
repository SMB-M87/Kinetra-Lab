# Kinetra-Lab Agent Guide

Rules for agents and developers working on Kinetra-Lab. **All rules in [Kinetra's AGENTS.md](external/Kinetra/AGENTS.md) apply here**: the development cycle, verification by impact, git, engineering rules, code and repository documentation, and skills. This file adds only what is specific to Lab. Explicit user instructions take precedence.

## Table of contents

- [Where to read what](#where-to-read-what)
- [Scope](#scope)
- [Lab boundaries](#lab-boundaries)
- [Verification in Lab](#verification-in-lab)
- [Git with the engine submodule](#git-with-the-engine-submodule)
- [Documentation in Lab](#documentation-in-lab)
- [Functionality index](#functionality-index)

[Project overview](README.md) · [Documentation index](docs/README.md)

## Where to read what

The Kinetra pages live in the `external/Kinetra` submodule. If that directory is empty, run `git submodule update --init --recursive` first.

| Task | Read |
| --- | --- |
| Every task | This file and [Kinetra's AGENTS.md](external/Kinetra/AGENTS.md) |
| Start, plan, review or hand over a change | [Workflow](external/Kinetra/docs/workflow.md); your branch's design document, and an older one only as [Kinetra's rule](external/Kinetra/AGENTS.md#development-cycle) allows |
| Build or review a change, so known mistakes are not repeated | Lab's [review lessons](docs/review-lessons.md) and [Kinetra's](external/Kinetra/docs/review-lessons.md) |
| Write or review C++ or CMake | [Conventions](external/Kinetra/docs/conventions.md) |
| Add tests or pick a verification level | [Testing](external/Kinetra/docs/testing.md) and [verification in Lab](#verification-in-lab) |
| Write READMEs, diagrams or `///` comments | [Documentation guide](external/Kinetra/docs/documentation.md) |
| Understand Lab targets and session flow | [System architecture](docs/architecture.md) |
| Work on one application | Its README and architecture page (see [functionality index](#functionality-index)) |
| Know what is planned, or record an idea | [Roadmap](docs/roadmap.md) (the only plan, in order of priority; `roadmap` skill), and the [Kinetra roadmap](external/Kinetra/docs/roadmap.md) for engine work |

## Scope

- Lab owns the runnable applications (Demo, Dotio, i4sim), the bundled launcher executable `kinetra`, and their tests and media.
- The engine stays in Kinetra. A mechanism that two applications need, or that has no game rules in it, is a candidate for Kinetra. Move it there with a Kinetra design document, and an ADR when it changes a boundary.

## Lab boundaries

Each application follows the [layer rule](external/Kinetra/docs/conventions.md#layers-and-dependencies):

| Layer | Targets | May depend on |
| --- | --- | --- |
| Logic: headless rules, state, window-free input mapping and debug-menu rows | `kinetra_demo_logic`, `kinetra_dotio_logic`, `kinetra_i4sim_logic` | `Kinetra::Simulation`, and the headless Lab policy in `kinetra_lab/input.h` and `kinetra_lab/debug.h` |
| Presentation: session, panels, drawing | `kinetra_demo_presentation`, `kinetra_dotio_presentation`, `kinetra_i4sim_presentation` | Its own logic, `Kinetra::Runtime`, shared Lab helpers, and `Kinetra::Launcher` for its menu widgets |
| Composition roots | `kinetra`, `kinetra_demo`, `kinetra_dotio`, `kinetra_i4sim`, `kinetra_sessions`, and the developer tool `kinetra_capture` | Presentation libraries, `Kinetra::Launcher`; `kinetra_capture` also `kinetra_sessions` and zlib |

- Input mapping that needs no window belongs in the logic layer, where it is tested headless: Demo's `demo/input/mapping.h` and i4sim's `FactoryController`. So do the rows of a debug menu, as `kinetra::DebugEntry` values: Demo's `DebugPanel`, Dotio's `Application::debug_entries()` and `FactoryController::menu()`. From `kinetra_lab`, logic may include only `kinetra_lab/input.h` and `kinetra_lab/debug.h`, which use Kinetra's plain input, camera and menu types; `kinetra_lab/settings.h` and `kinetra_lab/session.h` need the runtime.
- A presentation may link `Kinetra::Launcher` to reuse its menu widgets, as i4sim does with `LaunchMenu` for its blueprint screen. It never depends on the bundled launcher in `launcher/`.
- Applications never include each other's headers. Shared Lab code is application-neutral: the Lab policy in [labs/shared](labs/shared/include/kinetra_lab/) (target `kinetra_lab`) and the test helpers in [kinetra_lab/testing/test_support.h](labs/shared/tests/include/kinetra_lab/testing/test_support.h) (target `kinetra_lab_testing`).
- Files follow the [directory layout](external/Kinetra/docs/conventions.md#directory-layout): every module has one `include/<name>/` (`demo`, `dotio`, `i4sim`, `kinetra_lab`, `launcher`, `capture`) and one `src/` that mirrors it, presentation code in `presentation/`, every header that is not an entry of its module in a topic folder (at least two files per topic), internal headers in `detail/`, test helpers in `tests/include/<name>/testing/`. Include as `"i4sim/simulation/simulation.h"`; never `../`.
- Logic must build and test with `-DKINETRA_LAB_BUILD_RUNTIME=OFF`.
- i4sim needs the header-only nlohmann JSON package (3.11+). Its blueprints live in `labs/i4sim/blueprints/`; CMake copies them to `i4sim-blueprints/` next to every executable that opens them, and the application reads them from there. Treat blueprint files as a data format: changing their fields is a contract change.

*Why:* headless logic is what makes simulation behavior testable without a window, and the dependency direction keeps applications replaceable.

## Verification in Lab

Use the levels from [Kinetra testing](external/Kinetra/docs/testing.md#verification-levels):

At every level, run `python3 external/Kinetra/tools/check_copies.py`, with `--term '<old wording>'` for each rule, procedure or term the change rewords. It lists the CI jobs, CMake options, presets, flags and inline names the diff removes that a page in Lab or `external/Kinetra` still names; judge each hit ([tools](external/Kinetra/tools/README.md#check_copiespy)).

| Level | In Lab |
| --- | --- |
| Docs | `python3 external/Kinetra/tools/check_docs.py`. CI runs only this check for most docs-only pull requests, and none of them gets an agent review (below) |
| Targeted | `cmake --build --preset dev --target <targets>` and `ctest --preset dev -R '<regex>'`, using the table below (configure once with `cmake --preset dev`) |
| Full | Locally the targeted builds and tests of every changed area, from the table below; then the pull request's CI (format, docs, `headless`, `ci` with the graphics cases and the smoke runs of every executable on software OpenGL, the same on Ubuntu LTS, native Windows). Verified only when CI passes. Run `ci`, `headless`, `graphics` or `windows` locally only to reproduce a CI failure ([Kinetra testing](external/Kinetra/docs/testing.md#full)) |

| Changed area | Targets | CTest regex |
| --- | --- | --- |
| `labs/i4sim/` logic (`simulation/`, `blueprint/`, `controller`) | `kinetra_i4sim_tests kinetra_i4sim_golden_tests kinetra_i4sim_pressure_tests kinetra_i4sim_controller_tests kinetra_i4sim` | `^kinetra_i4sim` |
| `labs/demo/` logic (`application`, `debug_panel`, `world/`, `input/`) | `kinetra_demo_tests kinetra_demo_terrain_tests kinetra_demo_crowd_tests kinetra_demo_crowd_characterization_tests` | `^kinetra_demo(:\|_terrain\|_crowd)` |
| `labs/dotio/` logic (everything outside `presentation/`) | `kinetra_dotio_tests` | `^kinetra_dotio:` |
| `labs/shared/` | `kinetra_lab_tests` | `^kinetra_lab:` |
| `launcher/include/launcher/applications.h`, `launcher/src/applications.cpp` | `kinetra_menu_tests` | `^kinetra_menu:` |
| `tools/capture/` | `kinetra_capture_tests kinetra_capture`; then, as quick feedback, the smoke test in the `graphics` preset (`ctest --preset graphics -R kinetra_capture_smoke`), which CI also runs | `^kinetra_capture` |
| Any `presentation/` folder, launcher loop, `main.cpp` | Full level. Locally, build the area's `*_presentation_tests` (or `kinetra_runtime_tests`) and its executable in the `graphics` preset; running them there with `ctest --preset graphics -L graphics -R '<regex>'` is optional quick feedback, CI runs them | `<suite>_graphics\|kinetra_smoke:<name>` |
| `external/Kinetra` pointer or engine sources | Full level, plus Kinetra's own checks; a pointer move whose Kinetra range is only Markdown, `docs/`, `.agents/`, `.github/` or `tools/` is the Docs level, and CI runs only the docs check for it | |
| `tools/release/`, `.github/workflows/release.yml` | Docs level, plus the local cross build and `package_windows.sh` from the [release tool](tools/release/README.md#build-the-zip-locally); the workflow itself runs only after a merge on `main` or a tag | |
| Windows build (`cmake/toolchains/`, `cmake/WindowsDependencies.cmake`, Windows presets, `#ifdef _WIN32` code) | Full level; CI's `windows` job builds natively. Only a change to the WSL cross build itself (`cmake/toolchains/`, the `windows` preset) also needs `cmake --workflow --preset windows` from WSL, which CI does not run ([Windows](docs/windows.md)) | |

The Demo, Dotio, i4sim and settings tests are doctest suites: CTest names each case `<suite>:<case>`, so a regex that starts with `^<suite>` selects the whole suite and a longer one selects single cases. Add `-LE soak` to skip the five 60-second Demo crowd cases.

**Docs-only pull requests.** A pull request that changes only Markdown, `docs/` or `.agents/`, or such files and a pointer whose Kinetra range holds only Markdown, `docs/`, `.agents/`, `.github/` or `tools/`, which Lab's build does not read, is docs-only: it gets no agent review and no design document, and merges under [Kinetra's merge rule](external/Kinetra/AGENTS.md#git). CI's `changes` job skips its build and tests and runs only the docs check, except when it touches one of three Lab paths that a test reads or the build copies: `README.md` and `docs/media/` (`kinetra_capture_tests`), and `labs/i4sim/blueprints/`. Those build in full and still need no review. The decision is `tools/release/needs_build.sh`, the one the release workflow uses.

The level is decided by the whole change; one Full-level file lifts everything. In the `dev` preset (Debug), the large-factory cases of `kinetra_i4sim` and the Demo crowd soak cases take minutes. For a quick targeted run, use the `release` preset.

**Warnings.** `KINETRA_LAB_WARNINGS` (on in `dev` and `ci`) reports the shared warning set for `labs/` and `launcher/`. Engine warnings, and those of the warning-free Lab modules, are errors in CI's `linux (arch)` run and the `windows` job; the `linux (ubuntu)` run shows them as annotations (`KINETRA_WARNINGS_AS_ERRORS=OFF`, `KINETRA_LAB_WARNING_FREE_AS_ERRORS=OFF`), because Arch's pinned compiler is the reference.
- Lab code had 119 warnings when this was introduced. New or changed Lab code must not add any.
- When a module becomes warning-free, add `kinetra_directory_warnings(${CMAKE_CURRENT_SOURCE_DIR}/labs/<module> RECURSE ${warning_free_mode})` next to the others in the root `CMakeLists.txt`, so CI keeps it that way; a literal `AS_ERRORS` would ignore `KINETRA_LAB_WARNING_FREE_AS_ERRORS`.

## Git with the engine submodule

- Before starting work on a local branch, pull it first (`git pull --ff-only`; the HTTPS fallback from the global Git rules when SSH is unavailable), in Lab and in `external/Kinetra`. A local branch can lag behind its remote, for example `main` after a merged pull request or a feature branch after review commits.
- Lab records one exact Kinetra commit. A change to `external/Kinetra` is committed **in Kinetra** on its own branch, then the pointer is committed in Lab (`git add external/Kinetra`).
- Never commit a pointer to a Kinetra commit that is not on a Kinetra branch. Push the Kinetra branch and open its pull request before pushing Lab.
- The Lab pull request links the Kinetra pull request. It states that Kinetra merges first, with a merge commit rather than a squash, so the pointer stays reachable.
- Lab `main` records a commit on Kinetra `main`. The order is: the Kinetra pull request merges; then the Lab pull request merges with its pointer at the Kinetra branch commit it was verified with; then, right away, a separate **pointer-only pull request** records the Kinetra merge commit. *Why:* a pointer to the branch commit is reachable but not on `main`, so the submodule on `main` shows as modified in Lab until the pointer pull request merges.
- A pointer-only pull request changes nothing but `external/Kinetra`, to a commit on Kinetra `main`. It needs no agent review, and the agent merges it once its CI passes ([Kinetra rule](external/Kinetra/AGENTS.md#git)). CI skips build and tests for it when Kinetra changed only Markdown, `docs/`, `.agents/`, `.github/` or `tools/` between the two pointers, an identical tree included, and runs only the docs check then; any other Kinetra change runs the full CI. Never push the move to the Kinetra merge commit onto the Lab pull request that is open or reviewed: its CI would run again for one line. While the Kinetra pull request is in review and its branch moves, the Lab branch follows it with a pointer commit, as Kinetra's [stack rules](external/Kinetra/docs/workflow.md#stacked-pull-requests) say.
- The manual steps and commands are in [External dependencies](external/README.md).

## Documentation in Lab

- **Module READMEs:** `launcher/README.md`, `labs/demo/README.md`, `labs/dotio/README.md` and `labs/i4sim/README.md` are the module READMEs. They follow the [module template](external/Kinetra/docs/documentation.md#module-readme-template).
- **Thin sub-READMEs:** a navigation-only README in an `include/`, `src/` or `tests/` subdirectory is folded into its module README.
- **Architecture pages:** the cross-application pages live in [docs/](docs/README.md).
- **Design documents:** Lab design documents go in [docs/design/](docs/design/README.md). A docs-only change needs none, and a change across Kinetra and Lab has one, in the repository with most of it ([rule](external/Kinetra/AGENTS.md#development-cycle)).
- **Roadmap:** application, tool and build work that is planned or only an idea goes in [docs/roadmap.md](docs/roadmap.md); engine work in [Kinetra's roadmap](external/Kinetra/docs/roadmap.md), with a linked item in each when it needs both. Format in the [documentation guide](external/Kinetra/docs/documentation.md#roadmap).
- **ADRs:** Lab ADRs go in `docs/decisions/`, created with its README when the first Lab ADR is written.
- **Media:** screenshots and clips in [docs/media](docs/media/README.md) are refreshed with `tools/capture/capture_media.sh` ([capture tool](tools/capture/README.md)) when visible behavior changes; look at every refreshed file before committing it. The tour video is not in the repository: `tools/capture/capture_tour.sh` renders it into `.docs/`, which Git ignores, for an upload to YouTube, when the user asks for one, and refreshes the thumbnail in `docs/media`.

## Functionality index

| Application | Implementation | Tests | Documentation |
| --- | --- | --- | --- |
| Bundled launcher | [launcher/src](launcher/src/), [registrations](launcher/include/launcher/applications.h) | [launcher/tests](launcher/tests/) | [launcher](launcher/README.md), [launcher and overlays](docs/launcher-overlays.md) |
| Demo | [labs/demo](labs/demo/) | [labs/demo/tests](labs/demo/tests/) | [Demo](labs/demo/README.md) |
| Dotio | [labs/dotio](labs/dotio/) | [labs/dotio/tests](labs/dotio/tests/) | [Dotio](labs/dotio/README.md) |
| i4sim | [labs/i4sim](labs/i4sim/) | [labs/i4sim/tests](labs/i4sim/tests/) | [i4sim](labs/i4sim/README.md), [architecture](docs/factory-simulation-architecture.md) |
| Shared Lab policy (settings rows, debug rows, input, pause, session base) | [labs/shared](labs/shared/include/kinetra_lab/) | [shared tests](labs/shared/tests/shared_tests.cpp); the overlay itself in Kinetra | [launcher and overlays](docs/launcher-overlays.md) |
| Build | [CMakeLists.txt](CMakeLists.txt), [build.sh](build.sh) | `ctest --test-dir build` | [README](README.md#quick-start) |
| Windows release zip | [tools/release](tools/release/), [release.yml](.github/workflows/release.yml) | the workflow's DLL import check | [release tool](tools/release/README.md) |
| Media capture (screenshots and GIF clips) | [tools/capture](tools/capture/) | [capture tests](tools/capture/tests/) | [capture tool](tools/capture/README.md), [media guide](docs/media/README.md) |
