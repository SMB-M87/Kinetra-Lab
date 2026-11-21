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

Started on 2026-11-21. The [quality roadmap](design/2026-11-18-quality-roadmap.md) is merged up to phase 7 (phase 6 with #62); what it leaves is listed below: decisions and checks that are the user's, Demo's contact tests, and phase 8.

## Now

Nothing in progress.

## Next

Decisions and checks that only the user can make come first; they close phases 5 to 9.

- **Dotio playtest** · dotio · added 2026-11-21
  Phase 6 ends with a playtest by the user, and a review of the decisions the agent took alone (planner, Kinetra `search` API, whole routes, four random streams, golden fingerprint).
  Next step: the user plays with the [playtest checklist](design/2026-11-21-dotio-restructuring.md#playtest-checklist). Where: [Dotio restructuring, open points](design/2026-11-21-dotio-restructuring.md#open-points).
- **MAKE WAY for sealed-in movers** · i4sim · added 2026-11-21
  A mover sealed in by parked neighbors stands still for good; phase 7 documented it as a limit and proposed an optional MAKE WAY policy, off by default.
  Next step: the user decides whether to build it. Where: [phase 7, step 4](design/2026-11-21-i4sim-passing-under-pressure.md#step-4-why-two-movers-stand-still-near-a-destination).
- **Windows zip on a desktop** · release · added 2026-11-21
  Phase 9 is merged; its done-criterion is the user's: download `Kinetra-Lab-windows-x64.zip` from the README link and start the launcher on Windows without installing anything.
  Next step: the user's desktop check. Where: [Windows release zip](design/2026-11-21-windows-release-zip.md).
- **Frame pacing in WSLg** · runtime, build · added 2026-11-21
  The Linux build stutters in WSLg and the Windows build does not; the design document lists three options.
  Next step: the user chooses one. Where: [Frame pacing in WSLg](design/2026-11-20-wslg-frame-pacing.md) (draft).
- **i4sim session oddities** · i4sim · added 2026-11-21
  `R` drops a `--ups` rate, RESET PARAMETERS sets a horizon of 8 against the default 32, and `create_session` ignores its `output` argument.
  Next step: the user decides the intended behavior. Where: [i4sim open points](../labs/i4sim/README.md#open-points).
- **Demo's help overlay** · demo · added 2026-11-21
  `draw_help` and `toggle_help` are reachable only from tests.
  Next step: the user decides: remove them, or draw the legend. Where: [Demo open points](../labs/demo/README.md#open-points).
- **Demo's contact tests to Kinetra** · demo, tests, with Kinetra · added 2026-11-21
  Phase 5, step 4: Lab drops `labs/demo/tests/contact_tests.cpp` and `kinetra_demo_contact_tests` once Kinetra has the suite. The branches were committed in a session without Kinetra push access and never reached GitHub.
  Next step: redo the move from the plan, Kinetra first. Where: [Demo restructuring, step 4](design/2026-11-21-demo-restructuring.md#step-4-contact-tests-to-kinetra); Kinetra item *Demo's contact tests in Kinetra*.
- **Quality roadmap phase 8: shared passing mechanism** · i4sim, demo, dotio, with Kinetra · added 2026-11-21
  Run the phase 4 pressure scenarios against a Kinetra mechanism, and give Demo crowds and Dotio enemies the same mechanism. Phases 5 to 7 are done, so it can start.
  Next step: a design session; the MAKE WAY decision may shape it. Where: [phase 8](design/2026-11-18-quality-roadmap.md#phase-8-shared-passing-mechanism); Kinetra item *Shared passing mechanism*.

## Later

- **Close the quality roadmap** · docs · added 2026-11-21
  When phase 8 merges, set the [quality roadmap](design/2026-11-18-quality-roadmap.md) to `implemented`. Its status table still says phase 9 is in review; correct that then.
  Depends on: phase 8.
- **Compiler-sensitive golden fingerprints** · tests · added 2026-11-21
  Four Demo crowd characterization cases and i4sim's RVO golden case fail on Ubuntu's GCC 13 and pass on CI's Arch toolchain; they are already skipped on Windows. Decide on a tolerance or a per-toolchain fingerprint.
  Where: [Dotio restructuring, open points](design/2026-11-21-dotio-restructuring.md#open-points).
- **Dotio on `kinetra::Vec2`** · dotio · added 2026-11-21
  `dotio::math::Vec2` duplicates Kinetra's vector with extra operators and seven aliases.
  Where: [Dotio open points](../labs/dotio/README.md#open-points).
- **One i4sim session frame helper** · i4sim, capture · added 2026-11-21
  The two i4sim measurements repeat the session's frame order by hand; a helper in the i4sim logic would serve both.
  Where: [capture tool open points](../tools/capture/README.md#open-points).
- **Launcher `--seed`** · launcher · added 2026-11-21
  The bundled launcher cannot replay a run exactly; `kinetra_i4sim` and `kinetra_dotio` take a seed.
  Where: [launcher open points](../launcher/README.md#open-points).

## Ideas

Wishes of the user, not yet agreed or shaped. Each needs a design session (`grilling`) before it moves up.

- **Kinetra-Lab on Android** · launcher, release, with Kinetra · added 2026-11-21
  Run the launcher and the applications on an Android phone or tablet. Lab's part: touch-friendly menus and overlays, the blueprints of `i4sim-blueprints/` packaged as assets, and an APK build next to the Windows zip.
  Depends on: Kinetra's *Android runtime* idea (an OpenGL ES path, touch input, an NDK build).
- **3D Skatepark lab** · new application, with Kinetra · added 2026-11-21
  A fourth lab based on the user's [Skatepark](https://github.com/SMB-M87/Skatepark) repository: a skatepark built in C++ with OpenGL, freeglut, GLEW and glm, from procedurally generated cubes, prisms, cylinders, spheres and tori, a skateboard model from Blender, and a ground and an aerial camera.
  Depends on: Kinetra's *3D rendering* idea. Open questions: port Skatepark's geometry and camera, or rebuild them on Kinetra's 3D module; which parts are headless logic that can be tested without a window.
