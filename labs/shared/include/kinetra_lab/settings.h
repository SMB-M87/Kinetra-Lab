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
/// FRAME TIMES, FRAME LIMIT, SIM CONTROLS and DEBUG CONTROLS.
///
/// The four flags are borrowed by the rows and must outlive the overlay built from them.
/// While `frame_times` is set, the session draws kinetra::draw_frame_times over its frame;
/// `frame_limit` is the `enabled` flag of the session's FrameLimitSwitch. The help lines are
/// the application's own controls; DEBUG CONTROLS starts with debug_menu_help and
/// wheel_zoom_help. Zoom is not a setting: every application's debug menu has a ZOOM row
/// (kinetra_lab/debug.h).
inline std::vector<kinetra::SettingsRow> settings_rows(bool &statistics, bool &debug, bool &frame_times,
                                                       bool &frame_limit,
                                                       std::vector<std::string> sim_controls,
                                                       std::vector<std::string> debug_controls) {
    debug_controls.insert(debug_controls.begin(), {debug_menu_help, wheel_zoom_help});
    return {kinetra::ToggleRow{"STATISTICS", &statistics},
            kinetra::ToggleRow{"DEBUG MODE", &debug},
            kinetra::ToggleRow{"FRAME TIMES", &frame_times},
            kinetra::ToggleRow{"FRAME LIMIT", &frame_limit},
            kinetra::HelpSection{"SIM CONTROLS", std::move(sim_controls)},
            kinetra::HelpSection{"DEBUG CONTROLS", std::move(debug_controls)}};
}

/// A session's FRAME LIMIT setting: whether the runtime paces frames where the platform has
/// no VSync (kinetra::Runtime::limit_frames).
///
/// The runtime outlives sessions and owns the choice. The switch therefore starts from what
/// the runtime does, so a session opened from the launcher shows the choice made in the
/// session before it, and a host that switched the limit off (the capture tool) keeps it
/// off. Afterwards it passes on a change of `enabled`.
class FrameLimitSwitch {
  public:
    /// The FRAME LIMIT row's flag. Meaningless until the first apply().
    bool enabled{true};

    /// Call once per drawn frame, before drawing.
    void apply(kinetra::Runtime &runtime) {
        if (!synchronized_) {
            enabled = runtime.pacing() != kinetra::FramePacing::uncapped;
            synchronized_ = true;
        } else if (enabled != applied_)
            runtime.limit_frames(enabled);
        applied_ = enabled;
    }

  private:
    bool synchronized_{}, applied_{true};
};

} // namespace kinetra_lab
