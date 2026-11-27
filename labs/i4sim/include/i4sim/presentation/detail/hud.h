#pragma once
// The screen-space text of an i4sim frame: statistics, inspection, scene description and
// messages, and the RUN button of a blueprint that awaits its start.
#include "i4sim/simulation/simulation.h"

#include <kinetra/runtime/engine.h>
#include <kinetra/simulation/math.h>

#include <cstddef>
#include <optional>
#include <string_view>

namespace i4sim {
/// What the HUD shows besides the simulation's own state. The views are read during
/// draw_hud only.
struct HudContent {
    /// Whether the two statistics lines at the bottom show.
    bool statistics{true};
    /// Producer to inspect, by index into the producer list.
    std::optional<std::size_t> selected_producer;
    /// One line about the scene that runs; empty for none.
    std::string_view description;
    /// A message for the operator, such as a rejected goal; empty for none.
    std::string_view message;
    /// Ticks per second the clock reached, shown before the UPS it asks for (`UPS 9800/1000000`): lower when
    /// the processor cannot keep up or the run is paused.
    double measured_ups{};
};

/// Draws the HUD inside a frame that the caller began, in logical screen pixels: the
/// statistics on the two bottom lines, the inspection of the selection above them, the
/// description above that, and the message below the toolbar. Reads the simulation only.
/// Throws std::out_of_range for a selected index outside its list.
void draw_hud(kinetra::Runtime &runtime, const Simulation &simulation, const HudContent &content);

/// Draws the RUN button in `box`, screen pixels from start_button(), inside a frame that
/// the caller began: a panel with a play mark, RUN and CLICK OR SPACE in one row, in the
/// accent color when `lit` because the pointer is over it.
void draw_start_button(kinetra::Runtime &runtime, kinetra::Aabb box, bool lit);
} // namespace i4sim
