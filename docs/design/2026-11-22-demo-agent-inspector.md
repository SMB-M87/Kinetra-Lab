# Demo agent inspector

Status: implemented · [Inspector design](2026-11-22-agent-inspector-overlay.md)

## Design

Demo identifies a selected agent by its home, which survives remapping of physics body indices. A click on the agent opens its inspector. Clicking another switches selection. X clears selection, and eviction closes the panel. Clicks that set individual or group destinations can still run with a selection. In Ghost, clicking teleports as usual. RUNNING pauses the scene from the debug menu.

The headless trace takes the exact home coordinates shown in the wrapping CLI SELECTOR row and a seed. It samples the same builder after each 120 Hz tick.

## Verification

The application tests cover reselecting the same agent and closing the inspector in Ghost without a teleport. The graphics preset covers the normal session and debug rendering. A fixed-seed CLI run checks the JSON Lines shape with and without a resident agent.

## Review of `8259e07`

In `31b2022`, the seeded headless test supplies the wrapped selector and snapshots to the executable trace check. The session exercise covers selection, group goals, Ghost teleport, RUNNING and close. A separate headless test distinguishes a dormant agent from an evicted agent.

- `cmake --build --preset graphics --target kinetra_demo kinetra_demo_tests kinetra_demo_presentation_tests`
- `ctest --preset graphics -R '^kinetra_demo.*inspector' --output-on-failure` passed all five targeted cases.
- Real Demo session at seed 42, captured with the capture tool's Stage. [Screenshot](media/demo-agent-inspector.png)

## Pull request

**Title** `feat(demo): inspect selected agents and trace their state`

Second Lab step. Merge after the i4sim inspector. No further engine changes are needed.
