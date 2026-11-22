# Agent inspector overlay

Status: implemented · [Lab roadmap](../roadmap.md#next) · [Kinetra roadmap](../../external/Kinetra/docs/roadmap.md#next)

## Problem

Selecting an i4sim mover currently draws two lines in the [HUD](../../labs/i4sim/src/presentation/hud.cpp). They show speed, acceleration magnitude, goal and status. A headless run cannot read those lines, and the operator cannot see the remaining route, the temporary passing target or how long this mover has waited.

## Design and delivery order

1. **Kinetra.** Provide the grouped row model, screen layout, drawing and pointer capture described in the engine roadmap.
2. **i4sim.** Supply one snapshot for the mover selected by the controller. Show its index and blueprint ID in the title. The CLI accepts that index or the exact ID and outputs the canonical ID. Include the profile and tuning with units, lifecycle, operator hold, destination, remaining route, speed, steering components, contact ticks, distance travelled, and the active passing plan and waiting time. Keep the existing producer inspection. A change of blueprint clears the selection.
3. **Demo.** Keep individual destination commands, CLICK SETS GOAL and Ghost teleport available during inspection. Replace click-again and right-click deselection with the inspector's close action. Read the agent's current activity, motion settings, goal, route and last steering result. Check command, wander and sleep states against a seeded scene.
4. **Dotio.** Add enemy selection while preserving firing and Ghost teleport. The selection must expire when the enemy despawns or its pool slot is reused. Cover shooting, despawn and reuse before exposing this selection in the session.

The display and CLI use the same row builder at a completed tick. Opening the panel mid-run gives the same values as observing that mover since tick one. The CLI emits a sample per executed headless tick. Dotio uses the fixed input cadence documented in its integration. In i4sim it works with the existing headless blueprint, seed and tick options. An unknown mover ID gives an error before the run begins. Demo uses the home coordinates from the inspector title. Dotio uses the enemy pool slot and generation. Demo and Dotio use a prescribed seed and a neutral input stream. They emit null groups at ticks when the requested agent is absent.

## Interaction decisions

The inspector opens at the top right. The debug menu stays usable on the left. Each session has at most one inspected agent. Clicking a different agent replaces the contents of the open panel. Clicking the same agent again or clicking empty ground keeps the selection. The close button clears the selection and consumes that click. If the agent disappears or the session resets, the panel closes.

The inspector takes pointer input inside its bounds. Elsewhere each lab handles its own controls. In i4sim, Shift+click assigns the selected mover a goal and right-click still holds or releases a mover. Both actions take effect while the inspector is open. Inspecting a producer or toggling its blocked state should keep the inspected mover available for the next goal command.

The frame-time graph goes below the inspector when both are visible. In Dotio, Shift+right-click selects or switches the enemy under the pointer. In every lab, the debug menu's RUNNING control can pause or resume the simulation with the inspector open. The inspector keeps showing the selected agent while paused.

## Output decisions

| Question | Choice | Consequence |
| --- | --- | --- |
| CLI format | JSON Lines with tick, mover ID and ordered groups of display rows | A program can consume each tick independently and compare labels, values and units with the panel. The existing end-of-run summary must stay separate from this stream. |
| Waiting duration | Show both the current uninterrupted wait and accumulated waiting ticks per mover | Reuse the [flow metrics](../../labs/i4sim/include/i4sim/simulation/flow_metrics.h) definition. A held mover, a mover within 10 mm of its destination or one without an active goal accrues no waiting ticks. |

## Test plan

- Compare rows from a known blueprint before and after a tick, including units, remaining waypoints and a passing plan forming and ending. Compare simulation results with and without inspection.
- Exercise a mover waiting, resuming and waiting again. Select it after the first wait and check its history. Cover reset and a held mover.
- Run the headless executable on a fixture and compare samples with snapshots at the same ticks. Cover an unknown ID and an ordinary run without inspection.
- With the debug menu open, inspect A, click A again, click B, set B's goal with Shift+click, pause and resume through RUNNING, and close the inspector. Assert the selection and goal after each action, including a close click above a world entity. Exercise producer interaction and reset. In Demo cover individual and group goals. In Dotio cover Shift+right-click, firing, teleport and selected enemy removal.
- Enable the frame-time graph, open both panels, resize the window and scroll a long snapshot. Verify that the debug rows, the inspector's close button and the graph remain visible and that scrolling the inspector leaves world zoom unchanged. Run the graphics preset and bounded smoke runs for each integration.

## Open points

Stall diagnosis and recovery are separate items on the roadmap.

## Review

The first graphics run exposed goal clicks landing inside the inspector at 640×480. Limit the panel to the top half of the viewport. The center of the factory stays reachable and longer routes scroll. The existing traffic graphics tests and the new inspector pause test pass at their respective window sizes.

## Verification

The i4sim tests cover keeping a selection through producer clicks, explicit close, waiting totals against the flow benchmark and pausing from the debug menu. A headless run of the corridor showcase with `--inspect APM4220_west --ticks 20` produced 20 JSON records. The test also checked an unknown ID and the summary of a run without inspection.

### Review of `4d6ae44`

In `f09caef`, the display provides a numeric mover selector and the trace compares it with the exact mixed-case ID and direct simulation snapshots. The graph fits in the space available beside the debug menu. The waiting test now checks a second wait and reset. The graphics tests cover wheel capture at the active inspector and all three overlays at 320 × 240.

- `cmake --build --preset graphics --target kinetra_i4sim kinetra_i4sim_tests kinetra_i4sim_presentation_tests`
- `ctest --preset graphics -R 'inspector|head-on corridor standstill' --output-on-failure` passed seven targeted cases, including the equivalent selectors, unknown-ID error and ordinary batch output.
- Captured the paused predictive-passing showcase at seed 42 using the capture tool's Stage. [Screenshot](media/i4sim-agent-inspector.png)

## Pull request

**Title** `feat(i4sim): inspect movers and trace their state per tick`

Add one persistent mover selection, the shared inspector layout and JSON Lines tracing from the same snapshot. The panel exposes the active passing plan and both the current and cumulative wait.

Merge after [Kinetra #31](https://github.com/SMB-M87/Kinetra/pull/31), using a merge commit in Kinetra. Demo and Dotio build on this change.
