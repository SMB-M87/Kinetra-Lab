# build.sh builds Windows

Status: implemented · Branch: `build/build-sh-windows` · Related: [Windows](../windows.md), [Windows build with MinGW-w64](2026-11-19-windows-mingw.md)

`./build.sh` builds and tests Kinetra and Lab for Linux only. The Windows cross build needs a separate `cmake --workflow --preset windows`, so it is easy to forget after a change. This change makes `build.sh` also build the Windows executables whenever the cross compiler is installed.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

- [build.sh](../../build.sh) configures, builds and tests `build-core/` (Kinetra) and `build/` (Lab), both for Linux. Nothing in it touches Windows.
- The Windows cross build only runs when someone types `cmake --workflow --preset windows` ([Windows](../windows.md#quick-start-from-wsl)). CI does not run the cross build ([Windows CI](../windows.md#continuous-integration)), so a break in it can go unnoticed.

## Goals and non-goals

- Goal: `./build.sh` also builds the Windows executables in `build-windows/` when the cross build's tools are installed.
- Goal: on a machine without them, `./build.sh` still succeeds and says that it skipped Windows.
- Non-goal: running the Windows tests. That stays `ctest --preset windows` (user's choice).
- Non-goal: the native MSYS2 build (`windows-native`); `build.sh` is a Linux/WSL script.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| When to build Windows | Automatically when `x86_64-w64-mingw32-g++`, `ninja` and `ccache` are on `PATH`; otherwise skip with a message | User's choice; the script keeps working on a Linux machine without MinGW |
| Windows tests | Not run, build only | User's choice: faster, and the graphics tests would open windows on the Windows desktop |
| How to configure | The `windows` configure and build presets | They hold the toolchain file and options already; a second copy of them in the script would drift |

## Design

After the Linux builds, `build.sh`:

1. Checks the three tools the `windows` preset needs. If one is missing, it prints which ones and skips Windows; the exit status stays 0.
2. Refreshes a moved `build-windows/` cache the same way as `build-core/` and `build/`.
3. Runs `cmake --preset windows` and `cmake --build --preset windows`.

Ninja and ccache are checked as well because the preset uses them: a machine with MinGW but without Ninja would otherwise fail in the middle of the script instead of skipping.

## Affected areas

- [build.sh](../../build.sh).
- [README.md](../../README.md#tests-and-resources) (what `build.sh` does), [docs/windows.md](../windows.md#quick-start-from-wsl).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| With MinGW, Ninja and ccache, the script builds Linux and Windows and exits 0 | `./build.sh`; `build-windows/kinetra.exe` is rebuilt | Full (build script) |
| Without the cross compiler, the script skips Windows with a message and exits 0 | `./build.sh` with a `PATH` that hides `x86_64-w64-mingw32-g++` | Manual |
| The script is valid shell | `bash -n build.sh`, `shellcheck build.sh` if available | Docs-adjacent check |

The change touches only the build script and docs, no sources or CMake, so running the script itself is the Full check of what changed.

## Review

Self-review of `git diff main...HEAD`:

- `"${missing[*]}"` fails under `set -u` with an empty array in bash before 4.4. Fixed with `${missing[*]:-}`.
- The script was edited while a run was in progress during development; bash reads scripts incrementally, so verification was repeated on the final file.
- A failing Windows configure or build stops the script with a non-zero exit (`set -e`), so a broken cross build is reported, not hidden.
- The Windows build reports the known Lab warnings from LLP64 ([Windows limits](../windows.md#limits)); this change adds none.

## Verification

The change touches only the build script and docs, so running the script is the Full check of what changed.

| Command | Result |
| --- | --- |
| `bash -n build.sh` | No syntax errors (shellcheck is not installed here) |
| `./build.sh` with MinGW, Ninja and ccache present | Exit 0; Kinetra 200/200 and Lab 146/146 passed; `build-windows/` configured and built, `kinetra.exe` relinked |
| `./build.sh` with a `PATH` without `x86_64-w64-mingw32-*` | Exit 0; same test results; prints `Windows-build overgeslagen; ontbreekt: x86_64-w64-mingw32-g++` |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |

Not run: the Windows tests, by design (user's choice).

## Pull request

Title: `build: build the Windows executables in build.sh when MinGW is installed`

```markdown
## Summary
`./build.sh` now also builds the Windows executables in `build-windows/` after the Linux builds, when `x86_64-w64-mingw32-g++`, Ninja and ccache are on `PATH`. Without them it prints which tool is missing and skips Windows. It never runs the Windows tests.

## Changes
- `build.sh`: tool check, cache refresh for `build-windows/`, `cmake --preset windows` and `cmake --build --preset windows`.
- `README.md` and `docs/windows.md`: describe the Windows step.
- Design document `docs/design/2026-11-19-build-sh-windows.md`.

## Design
[docs/design/2026-11-19-build-sh-windows.md](docs/design/2026-11-19-build-sh-windows.md)

## Verification
- `./build.sh` with MinGW: exit 0, Kinetra 200/200, Lab 146/146, Windows build done.
- `./build.sh` with MinGW hidden from `PATH`: exit 0, same tests, Windows skipped with a message.
- `bash -n build.sh`, `python3 external/Kinetra/tools/check_docs.py`: pass.

## Review notes
- Empty-array expansion made safe under `set -u` for bash before 4.4.
- A failing Windows build stops the script with a non-zero exit.

## Dependencies and merge order
None; no Kinetra change.

## Follow-up
None.
```

## Open points

- None.
