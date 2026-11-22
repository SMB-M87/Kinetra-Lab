# Demo agent inspector

Status: implemented · [Inspector design](2026-11-22-agent-inspector-overlay.md)

## Design

Demo identifies a selected agent by its home, which survives remapping of physics body indices. A click on the agent opens its inspector. Clicking another switches selection. X clears selection, and eviction closes the panel. Clicks that set individual or group destinations can still run with a selection. In Ghost, clicking teleports as usual. RUNNING pauses the scene from the debug menu.

The headless trace takes the agent's home coordinates and a seed. It samples the same builder after each 120 Hz tick.

## Verification

The application tests cover reselecting the same agent and closing the inspector in Ghost without a teleport. The graphics preset covers the normal session and debug rendering. A fixed-seed CLI run checks the JSON Lines shape with and without a resident agent.

## Pull request

**Title** `feat(demo): inspect selected agents and trace their state`

Second Lab step. Merge after the i4sim inspector. No further engine changes are needed.
