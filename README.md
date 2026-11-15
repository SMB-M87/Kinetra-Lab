# Kinetra-Lab

Applications and experiments using the reusable [Kinetra](https://github.com/SMB-M87/Kinetra) core library. Launcher, Demo and Dotio live here; engine implementation stays in Kinetra.

## Table of contents

**Navigation:**

[Documentation index](docs/README.md)

Directory guides: [docs](docs/README.md), [external](external/README.md), [labs](labs/README.md), [launcher](launcher/README.md).

**On this page:**

- [Applications](#applications)
- [Quick start](#quick-start)
- [Kinetra development workflow](#kinetra-development-workflow)
- [Tests and resources](#tests-and-resources)
- [Architecture documentation](#architecture-documentation)
- [License](#license)

## Applications

See the [Labs overview](labs/README.md) for application sources and headless test commands.

| Directory | Targets | Purpose |
| --- | --- | --- |
| [launcher](launcher/README.md) | `kinetra`, `kinetra_sandbox` | Menu, registration and application switching |
| [labs/demo](labs/demo/README.md) | `kinetra_demo`, `kinetra_demo_presentation`, `kinetra_sandbox_application` | Terrain, navigation and physics reference |
| [labs/dotio](labs/dotio/README.md) | `kinetra_dotio`, `kinetra_dotio_presentation`, `kinetra_dotio_logic` | Survival game and AI experiment |

Kinetra supplies `Kinetra::Launcher` and its public `kinetra/launcher.h` header; Lab consumes that target. The engine checkout must include this target. During local development, configure with `-DKINETRA_SOURCE_DIR=/workspace/Kinetra` until the recorded submodule revision is updated.

## Quick start

Requires a C++20 compiler, CMake 3.16+, a build tool, SDL2 development files, OpenGL development files and an OpenGL 3.3 context. Linux is supported. On Artix/Arch use the normal `base-devel`, `cmake`, `sdl2` and `mesa` packages.

Repository layout:

```text
Kinetra-Lab/
├── docs/
├── launcher/
├── labs/
│   ├── demo/
│   └── dotio/
└── external/
    └── Kinetra/    # expected submodule checkout
```

The build uses the Git submodule at `external/Kinetra`. Clone with its sources:

```sh
git clone --recurse-submodules https://github.com/SMB-M87/Kinetra-Lab.git
cd Kinetra-Lab
```

For an existing clone, run `git submodule update --init --recursive` from Kinetra-Lab before configuring. `external/Kinetra/CMakeLists.txt` must exist. No separate Kinetra installation or build is needed.

Run from Kinetra-Lab:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/kinetra
./build/kinetra --variant demo --agents 12 --frames 120
./build/kinetra --variant dotio --difficulty medium --frames 120
./build/kinetra_demo --frames 120
./build/kinetra_dotio --difficulty medium --frames 120
```

Select with arrows/Enter or click; M returns to the menu, H toggles help, Backquote toggles debug controls. Each launch creates a fresh session. Detailed controls are in the application guides.

When Kinetra source changes, rebuild Lab; CMake rebuilds the affected core targets automatically. Run core tests separately in Kinetra. Use a fresh build directory when switching source checkouts.

## Kinetra development workflow

Kinetra-Lab and Kinetra have separate Git histories. Lab records one exact Kinetra commit, not a live branch. Editing `external/Kinetra` changes the engine checkout; it does not automatically publish those edits or update Lab's recorded commit. A sibling `../Kinetra` checkout is independent and does not synchronize automatically.

The Git commands below are manual developer steps. Inspect `git status` in both repositories before changing branches or updating submodules; commit or otherwise preserve local work first. Examples use `main`; use your feature branch and the normal review process when appropriate. Publish the Kinetra commit before publishing a Lab commit that references it, so other developers can fetch it.

### Change Kinetra from inside Lab

Submodules normally start with a detached HEAD. From Kinetra-Lab, create a branch before editing engine code:

```sh
git -C external/Kinetra switch -c my-engine-change
```

Edit reusable engine code in `external/Kinetra`; keep application changes in Lab. Build and test the engine independently, then build Lab against those same sources:

```sh
cmake -S external/Kinetra -B build-core -DCMAKE_BUILD_TYPE=Release
cmake --build build-core -j2
ctest --test-dir build-core --output-on-failure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The core build above tests the default headless configuration. For runtime changes, also run the runtime/graphics checks in [Kinetra development](external/Kinetra/docs/development.md#graphics-execution) and Lab's [graphics checks](#tests-and-resources).

Commit the intended engine files inside `external/Kinetra`, then publish that branch:

```sh
cd external/Kinetra
git diff
# Stage only the intended engine files, then:
git commit -m "Describe the engine change"
git push -u origin my-engine-change
cd ../..
```

After the Kinetra change is reviewed and merged, select its published upstream commit and rebuild Lab:

```sh
git -C external/Kinetra fetch origin
git -C external/Kinetra switch --detach origin/main
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

If merging changed the engine code, repeat the core checks too. Review and publish Lab's updated pointer, together with any required application changes:

```sh
git diff --submodule=log
git add external/Kinetra
# Stage any intended Lab source changes separately.
git commit -m "Update Kinetra dependency"
git push origin HEAD
```

When initially registering the submodule, also stage `.gitmodules` in the Lab commit. If the workflow uses pull requests, push to your Lab feature branch and open the normal review instead of updating a protected branch directly.

### Change Kinetra in the standalone checkout

Develop, test, commit and publish the engine change from the standalone Kinetra repository first. After it is merged into Kinetra's `main`, run from Kinetra-Lab with a clean submodule checkout:

```sh
git -C external/Kinetra fetch origin
git -C external/Kinetra switch --detach origin/main
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
git diff --submodule=log
```

Then commit and publish `external/Kinetra` in Lab using the pointer update steps above. Fetching alone does not change the checked-out engine version, and pushing Kinetra alone does not update Lab.

To test unpublished changes from a sibling checkout, use a separate build directory:

```sh
cmake -S . -B build-local-core -DCMAKE_BUILD_TYPE=Release -DKINETRA_SOURCE_DIR=../Kinetra
cmake --build build-local-core -j2
ctest --test-dir build-local-core --output-on-failure
```

This override does not update the submodule pointer. Before publishing Lab, select the published engine commit in `external/Kinetra` and verify the normal submodule build too. If `build` previously used an override, restore it explicitly with `-DKINETRA_SOURCE_DIR="$PWD/external/Kinetra"` when configuring.

### Receive published Lab changes

From Kinetra-Lab, preserve local changes before pulling, then restore the engine commit recorded by Lab:

```sh
git pull --ff-only
git submodule update --init --recursive
git submodule status
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

In `git submodule status`, a leading space means the checkout matches Lab's recorded commit; `-` means uninitialized, and `+` means a different commit is checked out. `git submodule update` selects Lab's recorded commit, not the latest upstream Kinetra commit. Lab application changes never flow into Kinetra automatically; make reusable engine changes in the engine repository and publish them separately.

## Tests and resources

`BUILD_TESTING` defaults to ON. `KINETRA_TEST_GRAPHICS=ON` adds presentation and launcher integration tests requiring a real OpenGL context:

```sh
cmake -S . -B build -DKINETRA_TEST_GRAPHICS=ON
cmake --build build --parallel
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build --output-on-failure
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./build/kinetra --variant demo --frames 120
```

Offscreen support depends on SDL/Mesa; alternatively use a desktop or Xvfb.
Software rendering does not establish desktop feel or performance.

For headless Demo/Dotio logic tests, configure a separate directory with `-DKINETRA_LAB_BUILD_RUNTIME=OFF`. Launcher and graphical executables are omitted in that configuration.

Shaders, bitmap text, terrain and drawing data are embedded or generated in code. No working-directory-specific assets or resource copy step is required.
Build directories and generated artifacts are ignored. Follow the included `.clang-format`; first-party C++ uses `.h` and `.cpp`.

## Architecture documentation

The [documentation index](docs/README.md) links system dependencies, application ownership and session/overlay interactions. Usage and controls live alongside each lab.

## License

MIT; see [LICENSE](LICENSE). Dotio attribution and port limits are documented in its [provenance](labs/dotio/README.md#provenance).
