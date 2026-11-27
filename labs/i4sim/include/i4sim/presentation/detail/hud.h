#pragma once
// The screen-space text of an i4sim frame: statistics, messages, inspection and scene
// description, and the RUN button of a blueprint that awaits its start.
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

/// HUD lines of the text above the statistics, counted from the bottom of the viewport as
/// kinetra::hud::line_top() counts them (1 is the lowest).
struct HudLines {
    /// Lowest line of the message, directly above the two statistics lines.
    int message{};
    /// The inspection of the selected producer, directly above the message.
    int inspection{};
    /// The scene description, one empty line above the inspection.
    int description{};
};

/// Where the HUD puts its text when the message takes `message_lines` lines (zero for none):
/// the statistics keep lines 1 and 2 even when hidden, the message comes next, and the
/// inspection and the description move up by its lines. Throws std::invalid_argument for a
/// negative count.
HudLines hud_lines(int message_lines);

/// Draws the HUD inside a frame that the caller began, in logical screen pixels, in the
/// shared HUD style (kinetra/runtime/hud_text.h): the statistics on the two bottom lines, the
/// message in red directly above them, wrapped to the window and at most four lines, then the
/// inspection of the selection and the description, placed by hud_lines(). Reads the
/// simulation only. Throws std::out_of_range for a selected index outside its list.
void draw_hud(kinetra::Runtime &runtime, const Simulation &simulation, const HudContent &content);

/// Draws the RUN button in `box`, screen pixels from start_button(), inside a frame that
/// the caller began: a panel with a play mark, RUN and CLICK OR SPACE in one row, in the
/// accent color when `lit` because the pointer is over it.
void draw_start_button(kinetra::Runtime &runtime, kinetra::Aabb box, bool lit);
} // namespace i4sim
