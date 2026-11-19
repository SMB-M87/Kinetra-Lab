# Window icon

Status: implemented · Branch: `feat/window-icon` · Related: [Kinetra design](../../external/Kinetra/docs/design/2026-11-19-window-icon.md), [Windows](../windows.md)

Lab's windows showed the desktop's empty default icon, and its Windows executables the generic program icon. Kinetra now draws a Kinetra icon on every `Runtime` window and offers `kinetra_executable_icon(<target>)` for Windows executables. This document covers the Lab half: the engine pointer and the icon on Lab's five applications.

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

- No Lab window has an icon: the engine never set one ([Kinetra design](../../external/Kinetra/docs/design/2026-11-19-window-icon.md#problem)).
- `kinetra.exe`, `kinetra_sandbox.exe`, `kinetra_demo.exe`, `kinetra_dotio.exe` and `kinetra_i4sim.exe` have no icon resource.

## Goals and non-goals

- Goal: every Lab window shows the Kinetra icon; the five application executables carry it on Windows.
- Non-goal: an icon per application (user's choice: one generated Kinetra icon).
- Non-goal: test and benchmark executables; nobody starts them from Explorer.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Icon | The generated Kinetra icon from the engine | User's choice ("Kan jij een icoon genereren?") |
| Executable icon on Windows | Yes, for the five applications | User's choice |
| Where the call sits | Next to each `add_executable` of an application | The composition roots own their executables; the helper is a no-op off Windows |

## Design

The window icon needs no Lab code: `Runtime` sets it. Each application's `CMakeLists.txt` calls `kinetra_executable_icon(<target>)` after `add_executable`, which adds the configured `kinetra_icon.rc` on Windows.

## Affected areas

- [launcher/CMakeLists.txt](../../launcher/CMakeLists.txt) (`kinetra`, `kinetra_sandbox`), [labs/demo/CMakeLists.txt](../../labs/demo/CMakeLists.txt), [labs/dotio/CMakeLists.txt](../../labs/dotio/CMakeLists.txt), [labs/i4sim/CMakeLists.txt](../../labs/i4sim/CMakeLists.txt).
- `external/Kinetra` pointer.
- [docs/windows.md](../windows.md).

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| The five application executables embed the icon; test executables do not | `cmake --preset windows`, build, then `x86_64-w64-mingw32-objdump -h` shows a `.rsrc` section and `windres -i <exe> -O rc` lists `RT_ICON` entries | Windows |
| Windows and sessions still open, draw and close | Existing graphics suites | Full with graphics |
| A running Lab window carries the icon on X11 | Manual: read `_NET_WM_ICON` of the launcher window | Manual |

The change moves the engine pointer and touches CMake, so the level is Full with graphics plus the Windows cross build.

## Review

Self-review of `git diff main...HEAD`:

- The helper is called only for application executables; `kinetra_i4sim_motion_benchmark` and the test executables keep no icon.
- `kinetra_i4sim` is also built headless; the helper does not depend on the runtime, so that build is unaffected.

## Verification

| Command | Result |
| --- | --- |
| `cmake --workflow --preset ci` | 140/140 passed |
| `cmake --workflow --preset headless` | 133/133 passed |
| `cmake --workflow --preset graphics` | 152/152 passed |
| `cmake --preset windows` and `cmake --build --preset windows` | Builds; the five application `.exe` files have a `.rsrc` section with `RT_ICON` entries, `kinetra_dotio_tests.exe` has none |
| `build-graphics/kinetra` in WSLg (XWayland), `_NET_WM_ICON` of the "Launcher" window read through libX11 | 64x64 icon present |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |

Not run: the Windows test suite (`ctest --preset windows`); the change adds only a resource to the executables, and the windows build links them. Native Wayland is not available here; SDL2 sets no icon there by design.

## Pull request

Title: `feat(labs): show the Kinetra icon on every window and executable`

**Summary.** Lab windows showed the desktop's empty default icon. With the new engine pointer every window shows the generated Kinetra icon, and the five Windows application executables embed it, so Explorer and shortcuts show it too.

**Changes.**
- `external/Kinetra` pointer: window icon in `Runtime`, `kinetra_executable_icon(<target>)`.
- `launcher`, `demo`, `dotio`, `i4sim` CMake: the icon on each application executable.
- Docs: Windows page.

**Design.** [docs/design/2026-11-19-window-icon.md](2026-11-19-window-icon.md), Kinetra: [window icon](../../external/Kinetra/docs/design/2026-11-19-window-icon.md).

**Verification.** `ci` 140/140, `headless` 133/133, `graphics` 152/152; Windows cross build with the icon resource in the five application executables; the window icon read back from X11 in WSLg; docs check.

**Review notes.** Native Wayland shows no window icon: SDL2 cannot set one there.

**Dependencies and merge order.** The Kinetra pull request first, with a merge commit; this branch records its commit.

**Follow-up.** None.

## Open points

None.
