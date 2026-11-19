# Native Windows build

Status: implemented · Branch: `docs/windows-native-build` · Related: [Windows](../windows.md), [Windows build with MinGW-w64](2026-11-19-windows-mingw.md)

[Windows](../windows.md) explains only the cross build from WSL. The user asked how to build from Windows itself. The `windows-native` preset and the MSYS2 CI job already do that, but no page tells a developer which shell, packages and commands to use.

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

- The `windows-native` preset in [CMakePresets.json](../../CMakePresets.json) is documented only as "the `ci` settings for a native MSYS2 shell".
- The packages and the submodule handling exist only in the `windows` job of [ci.yml](../../.github/workflows/ci.yml).
- Two traps are not written down: `.gitmodules` uses an SSH URL, and the native build links the GCC runtime and SDL2 dynamically, so its executables need the MSYS2 `bin` directory on `PATH`. The cross build links the runtime statically (`-static` in the [toolchain file](../../cmake/toolchains/x86_64-w64-mingw32.cmake)) and copies `SDL2.dll`.

## Goals and non-goals

- Goal: a developer on Windows can build, test and run Lab with the steps on one page.
- Goal: starting the WSL cross build from PowerShell is shown as the alternative.
- Non-goal: changing presets or CI. The native build behaves as before.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Where | A new section in [docs/windows.md](../windows.md) | The page owns the Windows build; no new page |
| Packages | Exactly the CI list | One package set to keep in sync, and CI proves it |

## Design

A section "Building on Windows" with a table of the two ways, the PowerShell command for the cross build, and the MSYS2 steps: shell, packages, line endings, clone with HTTPS rewrite, `windows-native` workflow, graphics tests on a desktop, and the `PATH` requirement.

## Affected areas

[docs/windows.md](../windows.md), [README.md](../../README.md), [docs/README.md](../README.md), this document and the design index.

## Test plan

Docs only: `python3 external/Kinetra/tools/check_docs.py`. Commands are copied from [ci.yml](../../.github/workflows/ci.yml) and [CMakePresets.json](../../CMakePresets.json).

## Review

Self-review of `git diff main...HEAD`:

- The `PATH` statement was checked against [CMakeLists.txt](../../CMakeLists.txt): `WindowsDependencies.cmake`, which copies `SDL2.dll`, is included only when cross-compiling.
- The binary directory `build-windows-native/` follows from the `base` preset's `build-${presetName}`.

## Verification

| Command | Result |
| --- | --- |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |

Not run: the MSYS2 steps on a Windows desktop. The `windows` CI job runs the same packages and workflow on every pull request.

## Pull request

Title: `docs(windows): explain the native MSYS2 build and the PowerShell route`

**Summary.** [docs/windows.md](../windows.md) now explains how to build from Windows: natively in an MSYS2 UCRT64 shell with the `windows-native` preset, or by starting the WSL cross build from PowerShell.

**Changes.**
- `docs/windows.md`: section "Building on Windows".
- `README.md`, `docs/README.md`: mention the native build.
- Design document.

**Design.** [docs/design/2026-11-19-windows-native-build.md](2026-11-19-windows-native-build.md).

**Verification.** Docs check passes. The MSYS2 steps mirror the `windows` CI job; they were not run on a desktop.

**Review notes.** The `PATH` note: native executables need `C:\msys64\ucrt64\bin`; the cross build does not.

**Dependencies and merge order.** None.

**Follow-up.** None.

## Open points

None.
