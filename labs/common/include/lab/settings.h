#pragma once

#include <kinetra/settings_view.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace lab {

/// Help lines that every Lab application shows under DEBUG CONTROLS before its own.
inline const std::string wheel_zoom_help = "WHEEL  ZOOM";

/// The settings rows every Lab application shows, in Lab's order: ZOOM, STATISTICS,
/// DEBUG MODE, FRAME TIMES, SIM CONTROLS and DEBUG CONTROLS.
///
/// `statistics`, `debug` and `frame_times` are borrowed by the rows and must outlive the
/// overlay built from them. While `frame_times` is set, the session draws
/// kinetra::draw_frame_times over its frame. `zoom` is read on each click, so it may depend on `debug`. The
/// help lines are the application's own controls; DEBUG CONTROLS starts with wheel_zoom_help.
inline std::vector<kinetra::SettingsRow> settings_rows(std::function<kinetra::ZoomLimits()> zoom,
                                                       bool &statistics, bool &debug, bool &frame_times,
                                                       std::vector<std::string> sim_controls,
                                                       std::vector<std::string> debug_controls) {
    debug_controls.insert(debug_controls.begin(), wheel_zoom_help);
    return {kinetra::ZoomRow{std::move(zoom)},
            kinetra::ToggleRow{"STATISTICS", &statistics},
            kinetra::ToggleRow{"DEBUG MODE", &debug},
            kinetra::ToggleRow{"FRAME TIMES", &frame_times},
            kinetra::HelpSection{"SIM CONTROLS", std::move(sim_controls)},
            kinetra::HelpSection{"DEBUG CONTROLS", std::move(debug_controls)}};
}

} // namespace lab
