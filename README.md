# Kinetra-Lab

Applications and experiments using the reusable [Kinetra](https://github.com/SMB-M87/Kinetra) core library. Launcher, Demo and Dotio live here; engine implementation stays in Kinetra.

## Table of contents

- [Applications](#applications)
- [Quick start](#quick-start)
- [Tests and resources](#tests-and-resources)
- [License](#license)

## Applications

| Directory | Targets | Purpose |
| --- | --- | --- |
| [launcher](launcher/README.md) | `kinetra`, `kinetra_sandbox`, `kinetra_launcher` | Menu, registration and application switching |
| [launcher/demo](docs/demo/README.md) | `kinetra_demo`, `kinetra_demo_presentation`, `kinetra_sandbox_application` | Terrain, navigation and physics reference |
| [launcher/dotio](docs/dotio/README.md) | `kinetra_dotio`, `kinetra_dotio_presentation`, `kinetra_dotio_logic` | Survival game and AI experiment |

The library target `Kinetra::Launcher` remains available inside Lab; its header is `launcher/include/kinetra/launcher.h`.

## Quick start

Requires a C++20 compiler, CMake 3.16+, a build tool, SDL2 development files, OpenGL development files and an OpenGL 3.3 context. Linux is supported. On Artix/Arch use the normal `base-devel`, `cmake`, `sdl2` and `mesa` packages.

Repository layout:

```text
Kinetra-Lab/
├── docs/
├── launcher/
│   ├── demo/
│   └── dotio/
└── external/
    └── Kinetra/    # expected submodule checkout
```

The build defaults to `external/Kinetra`. Submodule registration is still pending; until it is registered, use an existing checkout with `-DKINETRA_SOURCE_DIR=../Kinetra` on the first configure command below. No package installation is needed.

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

## License

MIT; see [LICENSE](LICENSE). Dotio attribution and port limits are documented in its [provenance](docs/dotio/README.md#provenance).
