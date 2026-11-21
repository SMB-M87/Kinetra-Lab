# Roadmap

The backlog of Kinetra-Lab: application, tool and build work that is wanted but not finished, from agreed follow-ups to loose ideas. Work in progress is described in its design document; this page says what comes after it. Engine work lives in the [Kinetra roadmap](../external/Kinetra/docs/roadmap.md).

## Table of contents

- [How to use this page](#how-to-use-this-page)
- [Now](#now)
- [Next](#next)
- [Later](#later)
- [Ideas](#ideas)

[Documentation index](README.md) · [Project overview](../README.md) · [Design documents](design/README.md)

## How to use this page

Sections, item format and the moves between sections are defined in the [documentation guide](../external/Kinetra/docs/documentation.md#roadmap). The `roadmap` skill adds, shapes and closes items, for example during a brainstorm with the user ([skill](../external/Kinetra/.agents/skills/roadmap/README.md)). A merged item is removed; its design document is the record.

Started on 2026-11-21, while the [quality roadmap](design/2026-11-18-quality-roadmap.md) still runs. Its phases 1 to 4 and 9 are merged; the phases that remain are items below, and the quality roadmap keeps their detailed scope.

## Now

- **Quality roadmap phase 5: Demo** · demo · added 2026-11-21
  Function bodies out of headers, `Sandbox` operations instead of public fields, a renderer parameter object, named zoom limits, dead code removed, module README.
  Where: branch `refactor/demo-scene-operations`; [phase 5](design/2026-11-18-quality-roadmap.md#phase-5-demo).
- **Quality roadmap phase 6: Dotio** · dotio, with Kinetra · added 2026-11-21
  Seeded, instance-owned state and `kinetra::Navigation` instead of Dotio's own A*, plus invariant tests. Ends with a playtest by the user against the recorded baseline.
  Where: branches `test/dotio-baseline` and `refactor/dotio-navigation`; [phase 6](design/2026-11-18-quality-roadmap.md#phase-6-dotio); Kinetra item *Navigation search for Dotio*.
- **Quality roadmap phase 7: predictive passing under pressure** · i4sim · added 2026-11-21
  Cause of Improved's extra contacts, guarded maneuvers, a measured comparison, and the standstills near a destination.
  Where: branches `test/i4sim-passing-causes` and `fix/i4sim-guarded-passing` (pull request #49); [phase 7](design/2026-11-18-quality-roadmap.md#phase-7-predictive-passing-under-pressure).

## Next

- **Windows zip on a desktop** · release · added 2026-11-21
  Phase 9 is merged, but its done-criterion needs the user: download `Kinetra-Lab-windows-x64.zip` from the README link and start the launcher on a Windows machine without installing anything. First check that the release run on `main` published the zip.
  Next step: the user's desktop check. Where: [phase 9](design/2026-11-18-quality-roadmap.md#phase-9-easier-try-out), [Windows release zip](design/2026-11-21-windows-release-zip.md).
- **Frame pacing in WSLg** · runtime, build · added 2026-11-21
  The Linux build stutters in WSLg and the Windows build does not; the design document lists three options.
  Next step: the user chooses an option. Where: [Frame pacing in WSLg](design/2026-11-20-wslg-frame-pacing.md) (draft).
- **i4sim session oddities** · i4sim · added 2026-11-21
  `R` drops a `--ups` rate, RESET PARAMETERS sets a horizon of 8 against the default 32, and `create_session` ignores its `output` argument.
  Next step: the user decides the intended behavior of each. Where: [i4sim open points](../labs/i4sim/README.md#open-points).

## Later

- **Quality roadmap phase 8: shared passing mechanism** · i4sim, demo, dotio, with Kinetra · added 2026-11-21
  Run the phase 4 pressure scenarios against the Kinetra mechanism, and give Demo crowds and Dotio enemies the same mechanism. Kinetra's part is its item *Shared passing mechanism*.
  Depends on: phases 5, 6 and 7. Where: [phase 8](design/2026-11-18-quality-roadmap.md#phase-8-shared-passing-mechanism).
- **Close the quality roadmap** · docs · added 2026-11-21
  When phase 8 merges, set the [quality roadmap](design/2026-11-18-quality-roadmap.md) to `implemented` and move its remaining open points here or to their module pages.
  Depends on: phase 8.
- **Reproducible Dotio media** · capture, dotio · added 2026-11-21
  Once Dotio's randomness is seeded, the Dotio clips and the tour's Dotio chapter can use fixed times instead of a take chosen by eye.
  Depends on: phase 6. Where: [capture tool open points](../tools/capture/README.md#open-points).
- **One i4sim session frame helper** · i4sim, capture · added 2026-11-21
  The two i4sim measurements repeat the session's frame order by hand; a helper in the i4sim logic would serve both.
  Where: [capture tool open points](../tools/capture/README.md#open-points).
- **Launcher `--seed`** · launcher · added 2026-11-21
  The bundled launcher cannot replay a run exactly; only `kinetra_i4sim` takes a seed.
  Where: [launcher open points](../launcher/README.md#open-points).

## Ideas

Wishes of the user, not yet agreed or shaped. Each needs a design session (`grilling`) before it moves up.

- **Kinetra-Lab on Android** · launcher, release, with Kinetra · added 2026-11-21
  Run the launcher and the applications on an Android phone or tablet. Lab's part: touch-friendly menus and overlays, the blueprints of `i4sim-blueprints/` packaged as assets, and an APK build next to the Windows zip.
  Depends on: Kinetra's *Android runtime* idea (an OpenGL ES path, touch input, an NDK build).
- **3D Skatepark lab** · new application, with Kinetra · added 2026-11-21
  A fourth lab based on the user's [Skatepark](https://github.com/SMB-M87/Skatepark) repository: a skatepark built in C++ with OpenGL, freeglut, GLEW and glm, from procedurally generated cubes, prisms, cylinders, spheres and tori, a skateboard model from Blender, and a ground and an aerial camera.
  Depends on: Kinetra's *3D rendering* idea. Open questions: port Skatepark's geometry and camera, or rebuild them on Kinetra's 3D module; which parts are headless logic that can be tested without a window.
