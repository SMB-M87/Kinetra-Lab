#pragma once

#include <kinetra/runtime/settings_view.h>

#include <string>
#include <utility>
#include <vector>

namespace kinetra_lab {

/// Help lines that every Lab application shows under DEBUG CONTROLS before its own.
inline const std::string debug_menu_help = "SHIFT + CTRL  DEBUG MENU";
inline const std::string wheel_zoom_help = "WHEEL  ZOOM";

/// The settings rows every Lab application shows, in Lab's order: STATISTICS, DEBUG MODE,
/// FRAME TIMES, SIM CONTROLS and DEBUG CONTROLS. The frame rate is the FPS row of the debug
/// menu (kinetra::FrameRateSetting).
///
/// The three flags are borrowed by the rows and must outlive the overlay built from them.
/// While `frame_times` is set, the session draws kinetra::draw_frame_times over its frame. The help lines are
/// the application's own controls; DEBUG CONTROLS starts with debug_menu_help and
/// wheel_zoom_help. Zoom is not a setting: every application's debug menu has a ZOOM row
/// (kinetra_lab/debug.h).
inline std::vector<kinetra::SettingsRow> settings_rows(bool &statistics, bool &debug, bool &frame_times,
                                                       std::vector<std::string> sim_controls,
                                                       std::vector<std::string> debug_controls) {
    debug_controls.insert(debug_controls.begin(), {debug_menu_help, wheel_zoom_help});
    return {kinetra::ToggleRow{"STATISTICS", &statistics}, kinetra::ToggleRow{"DEBUG MODE", &debug},
            kinetra::ToggleRow{"FRAME TIMES", &frame_times},
            kinetra::HelpSection{"SIM CONTROLS", std::move(sim_controls)},
            kinetra::HelpSection{"DEBUG CONTROLS", std::move(debug_controls)}};
}

} // namespace kinetra_lab
