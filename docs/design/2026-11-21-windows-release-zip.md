# Windows release zip

Status: implemented · Roadmap: [phase 9, easier try-out](2026-11-18-quality-roadmap.md#phase-9-easier-try-out) · Stacked on: [HTTPS submodule by default](2026-11-21-https-submodule.md)

[Design index](README.md)

## Problem and design

Seeing the launcher required a compiler, CMake, SDL2, nlohmann JSON and a clone with the submodule. The cross build already produces executables that need only `SDL2.dll` and `i4sim-blueprints/` beside them ([Windows](../windows.md)); nobody published them.

- **Packaging.** `tools/release/package_windows.sh <build-dir> <zip>` collects the launcher, the three applications, `SDL2.dll`, `i4sim-blueprints/`, the four licenses and a `README.txt` with the commit, and zips them with `cmake -E tar` (no extra tool).
- **Workflow.** `.github/workflows/release.yml` runs on a push to `main`, on a tag `v*` and by hand. Its `package` job uses the pinned Arch image of CI with `mingw-w64-gcc`, builds the four application targets of the `windows` preset, checks that the executables import only Windows libraries and `SDL2.dll`, and uploads the zip as an artifact. Its `publish` job (the only one with `contents: write`) recreates the `main-build` pre-release for `main`, or creates the release for a tag.
- **README.** The quick start opens with the download link.

### Decisions

| Question | Choice | Why |
| --- | --- | --- |
| Where the zip lives | A GitHub Release asset, not a file in the repository | A binary in Git grows the history on every build; a release asset is a separate download with a fixed URL |
| When it is built | Every push to `main` (pre-release `main-build`, recreated) and every tag `v*` | Recommended to the user on 2026-11-21: the README link works from the first merge, and tags give fixed versions |
| Which build | The MinGW-w64 cross build, not the native MSYS2 build | The native build links MSYS2's shared runtime and needs `C:\msys64\ucrt64\bin` on `PATH` |
| Tests | Not run in the release workflow | The pull request's CI ran them, natively on Windows included; doctest discovery of the test `.exe` files cannot run on Linux |

## Test plan and review

- Local cross build on Ubuntu 24.04 (`g++-mingw-w64-x86-64-posix`): the four application targets build; `objdump` shows only `KERNEL32.dll`, `msvcrt.dll` and `SDL2.dll` (Arch's toolchain links the UCRT instead, which the check also allows).
- `package_windows.sh build-windows dist/Kinetra-Lab-windows-x64.zip` produces 25 entries: four executables, `SDL2.dll`, the blueprints with `showcase/`, four licenses and `README.txt`.
- The workflow parses as YAML. It runs only after a merge, so its first run is the first merge on `main`: check that `main-build` appears with the zip.
- Open, for the user: start the downloaded `kinetra.exe` on a Windows desktop and open i4sim, which reads the blueprints. Neither the container nor CI has an OpenGL 3.3 driver on Windows.

## Pull request

**build(release): publish a ready-to-run Windows zip**

Adds `.github/workflows/release.yml` and `tools/release/`. Every merge on `main` refreshes the `main-build` pre-release with `Kinetra-Lab-windows-x64.zip`, and a tag `v*` creates a fixed release. The zip holds the launcher, Demo, Dotio, i4sim, `SDL2.dll`, the i4sim blueprints and the licenses, built with the MinGW-w64 cross toolchain on CI's pinned Arch image. The README quick start now starts with the download.

Stacked on the HTTPS submodule pull request; merge that first. To check after merging: the `Release` workflow run on `main`, and `kinetra.exe` from the zip on a Windows desktop.
