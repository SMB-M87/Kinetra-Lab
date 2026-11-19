#pragma once

#include <kinetra/view/camera.h>
#include <kinetra/view/debug_menu.h>

#include <algorithm>
#include <string>
#include <utility>

/// Debug-menu rows every Lab application shows. Headless: it uses only Kinetra's plain menu
/// and camera types, so an application's logic layer builds and tests its menu with them.
namespace kinetra_lab {

/// The ZOOM row of a debug menu.
///
/// The camera belongs to the host, and a menu row runs before the frame's camera update, so
/// the row only collects the steps its buttons ask for. Once per frame the application adds
/// take_steps() to the wheel's steps for kinetra::zoom, and calls observe() with the result,
/// which the row then shows. Not copyable, because the row refers to it.
class ZoomControl {
  public:
    ZoomControl() = default;
    ZoomControl(const ZoomControl &) = delete;
    ZoomControl &operator=(const ZoomControl &) = delete;

    /// The row: "ZOOM", the zoom last observed in pixels per world unit, and `[-]` `[+]`
    /// buttons of one zoom step each. At depth 0, below a rule.
    kinetra::DebugEntry entry() {
        return {.label = "ZOOM",
                .value = [this] { return kinetra::debug_number_text(shown_); },
                .decrease = [this] { --steps_; },
                .increase = [this] { ++steps_; },
                .rule_above = true};
    }
    /// The zoom steps the buttons asked for since the last call; positive zooms in.
    float take_steps() { return std::exchange(steps_, 0.f); }
    /// Remembers the camera's zoom for the row to show.
    void observe(const kinetra::Camera2D &camera) { shown_ = camera.pixels_per_unit; }

  private:
    float shown_{}, steps_{};
};

/// Slowest and fastest time scale of the SPEED row. The applications' fixed clocks run at
/// most eight frames' worth of ticks in one frame, so four leaves room for a frame that
/// takes twice as long before simulated time falls behind the scale shown.
inline constexpr float slowest_time_scale = 0.125f, fastest_time_scale = 4;

/// The SPEED row of a debug menu over `scale`, the factor between simulated and real time,
/// which the row borrows: it shows the factor ("0.5X") and its buttons halve and double it
/// within [slowest_time_scale, fastest_time_scale].
inline kinetra::DebugEntry time_scale_entry(float &scale, int depth = 1) {
    return {.label = "SPEED",
            .value = [&scale] { return kinetra::debug_number_text(scale) + "X"; },
            .decrease = [&scale] { scale = std::max(slowest_time_scale, scale / 2); },
            .increase = [&scale] { scale = std::min(fastest_time_scale, scale * 2); },
            .depth = depth};
}

} // namespace kinetra_lab
