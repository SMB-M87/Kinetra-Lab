#pragma once
// The clips a Tour plays in the real launcher, with its pointer, click rings and keycaps:
// the shared launcher and overlays in Demo, for Kinetra's README.
namespace capture {
class Stage;

/// The launcher menu with the pointer over each entry, DEMO, its configuration page with the
/// pointer over the scenarios, the start of DOORWAY, SCENARIOS in the settings panel back to
/// the configuration page with DOORWAY selected, and the page's LAUNCHER back to the list.
void ui_launcher(Stage &stage);
/// In Doorway: Ctrl opens the settings panel, STATISTICS goes off and on, SIM CONTROLS and
/// DEBUG CONTROLS open their help and close it, and RESUME closes the panel.
void ui_settings(Stage &stage);
/// In Doorway: Shift+Ctrl opens the debug menu, SPEED and MAX SPEED step up, AVOIDANCE
/// switches to LEGACY and back, LAYERS opens, and Shift+Ctrl closes the menu.
void ui_debug_menu(Stage &stage);
/// In Doorway: a click on an agent opens the inspector, FRAME TIMES in the settings panel
/// puts the frame-time graph below it, a click selects another agent, and X closes it.
void ui_inspector(Stage &stage);
/// In Terrain, with agents that wander by planned routes: every layer of the debug menu off
/// and on again, HOME RANGE, and the wheel zooming out.
void ui_debug_layers(Stage &stage);
} // namespace capture
