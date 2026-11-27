#pragma once

#include <kinetra/view/camera.h>
#include <kinetra/view/debug_menu.h>

#include <algorithm>
#include <cstdint>
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

/// Slowest and fastest UPS of every Lab application's UPS row, in ticks per wall-clock
/// second. A million is far above what the simulations reach; the rate the clock achieves
/// then falls behind the setting, and the statistics show by how much.
inline constexpr std::uint64_t slowest_ups = 1, fastest_ups = 1000000;

/// Share of a frame's interval (kinetra::FrameRateSetting::interval) that a frame's ticks may
/// use, for kinetra::TickBudget: the rest is for drawing and the window system, so the frame
/// rate holds when the UPS asks for more than the processor can do.
inline constexpr double tick_share = 0.75;

/// The UPS row of a debug menu over `ups`, which the row borrows: it shows the ticks per
/// second, its buttons halve and double them, and a typed number sets them, within
/// [slowest_ups, fastest_ups] (kinetra::debug_rate).
inline kinetra::DebugEntry ups_entry(std::uint64_t &ups, int depth = 1) {
    return kinetra::debug_rate(
        "UPS", [&ups] { return ups; }, [&ups](std::uint64_t value) { ups = value; }, slowest_ups, fastest_ups,
        depth);
}

} // namespace kinetra_lab
