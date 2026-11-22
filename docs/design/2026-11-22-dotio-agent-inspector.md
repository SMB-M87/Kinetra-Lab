# Dotio agent inspector

Status: implemented · [Inspector design](2026-11-22-agent-inspector-overlay.md)

## Design

Shift+right-click picks a live enemy from the displayed positions. The inspector remembers the pool slot and its generation so the selection expires when an enemy retires or a new enemy takes the slot. The panel shows the AI state, movement parameters and steering, plus its remaining route or heading where applicable. Left-click remains available for firing and Ghost teleport. Use RUNNING in the debug menu to pause during inspection.

The headless trace uses the slot and generation from the inspector title to observe the same rows after each 60 Hz update. Its fixed viewport is 1280 × 800, it uses neutral input, and it advances one update per frame. Equivalence is tested for this cadence. A graphical frame with multiple updates credits its spawning budget before running those updates.

## Verification

The application tests cover inactive and reused slots. The graphics test sends a normal right-click, Shift+right-click, a debug pause and a close in the real session. A fixed-seed batch run verifies JSON records before and after the first enemy spawns.

## Review of `feaf1d7`

In `0824a0b`, the headless test observes the same seeded ticks as the fixed-cadence trace and compares the complete records. A second command checks null rows for the wrong lifetime. The session test fires and teleports outside the inspector, then positions its close button over another verified free teleport destination and checks that the click leaves the player in place.

- `cmake --build --preset graphics --target kinetra_dotio kinetra_dotio_tests kinetra_dotio_presentation_tests kinetra_capture`
- `ctest --preset graphics -R '^kinetra_dotio.*inspector' --output-on-failure` passed all five targeted cases.
- Real Dotio session at the capture seed with Shift+right-click selection, captured through Stage. [Screenshot](media/dotio-agent-inspector.png)

## Pull request

**Title** `feat(dotio): inspect enemies with Shift and right-click`

Third Lab step. Merge after the Demo inspector. No further engine changes are needed.
