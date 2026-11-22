# Dotio agent inspector

Status: implemented · [Inspector design](2026-11-22-agent-inspector-overlay.md)

## Design

Shift+right-click picks a live enemy from the displayed positions. The inspector remembers the pool slot and its generation so the selection expires when an enemy retires or a new enemy takes the slot. The panel shows the AI state, movement parameters and steering, plus its remaining route or heading where applicable. Left-click remains available for firing and Ghost teleport. Use RUNNING in the debug menu to pause during inspection.

The headless trace uses the slot and generation from the inspector title to observe the same rows after each 60 Hz update.

## Verification

The application tests cover inactive and reused slots. The graphics test sends a normal right-click, Shift+right-click, a debug pause and a close in the real session. A fixed-seed batch run verifies JSON records before and after the first enemy spawns.

## Pull request

**Title** `feat(dotio): inspect enemies with Shift and right-click`

Third Lab step. Merge after the Demo inspector. No further engine changes are needed.
