# build.sh builds Windows

Status: accepted · Branch: `build/build-sh-windows` · Related: [Windows](../windows.md), [Windows build with MinGW-w64](2026-11-19-windows-mingw.md)

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

## Verification

## Pull request

## Open points
