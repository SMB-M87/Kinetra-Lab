#include "i4sim/presentation/detail/hud.h"

#include <kinetra/runtime/bitmap_text.h>
#include <kinetra/runtime/hud_text.h>
#include <kinetra/runtime/overlay_style.h>

#include <array>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace i4sim {
namespace {
using namespace kinetra;

/// The scene description stands out from the statistics without reading as an error.
constexpr Color description_color{0.75f, 0.85f, 1};
/// HUD lines the statistics keep, shown or not, so the message does not jump when they toggle.
constexpr int statistics_lines = 2;
/// An i4sim message is a sentence, or a path with its parse error, which four lines hold at
/// a common window width. A longer one is cut; its full text is on stderr and in the status.
constexpr int max_message_lines = 4;
/// Empty HUD line between the inspection and the description, as before the message moved here.
constexpr int description_gap = 2;

/// Run totals, with the counters of the coordination features that are on.
std::string totals_text(const Simulation &simulation, double measured_ups) {
    const auto &motion = simulation.motion_settings();
    std::ostringstream text;
    // Reached, then asked for, as in Demo; the bitmap font has no parentheses.
    text << "TICK " << simulation.ticks() << " UPS " << std::llround(measured_ups) << "/"
         << simulation.updates_per_second() << " CONTACTS " << simulation.collisions() << " DURATION "
         << simulation.contact_ticks();
    if (motion.predictive_passing && supports_predictive_passing(motion.profile))
        text << " PASS " << simulation.predictive_pairs();
    if (motion.corridor_traffic)
        text << " RETREAT " << simulation.corridor_recovering();
    return text.str();
}

/// Run state and motion settings, with the wait counters of the traffic policies that are on.
std::string motion_text(const Simulation &simulation) {
    const auto &motion = simulation.motion_settings();
    std::ostringstream text;
    text << (simulation.running() ? "RUNNING" : "PAUSED") << " SPEED " << motion.speed << " FORCE "
         << motion.force << " HORIZON " << motion.horizon;
    if (motion.profile == SteeringProfile::reciprocal)
        text << " AVOID LIMIT " << simulation.avoidance_limited();
    if (motion.parking_traffic)
        text << " PARK WAIT " << simulation.parking_waiting();
    if (motion.junction_traffic)
        text << " CROSS WAIT " << simulation.junction_waiting() << "/" << simulation.junction_count();
    if (motion.corridor_traffic)
        text << " LANE WAIT " << simulation.corridor_waiting() << "/" << simulation.corridor_count();
    if (motion.junction_traffic || motion.corridor_traffic)
        text << " EXIT " << simulation.junction_exit_waiting();
    return text.str();
}

void draw_producer_inspection(Runtime &runtime, const Simulation &simulation, std::size_t index, int line) {
    const auto &producer = simulation.producers().at(index);
    hud::draw_line(runtime,
                   producer.definition.id + " | QUEUE " + std::to_string(producer.queue.size()) +
                       " | REMAINING " + std::to_string(producer.remaining) + " | TASKS " +
                       std::to_string(producer.interactions) + (producer.blocked ? " | BLOCKED" : ""),
                   line, hud::statistics_color);
}
} // namespace

HudLines hud_lines(int message_lines) {
    if (message_lines < 0)
        throw std::invalid_argument("A message cannot have a negative number of lines");
    HudLines lines;
    lines.message = statistics_lines + 1;
    lines.inspection = lines.message + message_lines;
    lines.description = lines.inspection + description_gap;
    return lines;
}

void draw_hud(Runtime &runtime, const Simulation &simulation, const HudContent &content) {
    if (content.statistics) {
        hud::draw_line(runtime, totals_text(simulation, content.measured_ups), 2, hud::statistics_color);
        hud::draw_line(runtime, motion_text(simulation), 1, hud::statistics_color);
    }
    // The message's line comes from hud_lines(), so its test covers where the message is drawn.
    const auto lines =
        hud_lines(hud::draw_error(runtime, content.message, hud_lines(0).message, max_message_lines));
    if (content.selected_producer)
        draw_producer_inspection(runtime, simulation, *content.selected_producer, lines.inspection);
    if (!content.description.empty())
        hud::draw_line(runtime, content.description, lines.description, description_color);
}

void draw_start_button(Runtime &runtime, Aabb box, bool lit) {
    using namespace kinetra::overlay;
    const Vec2 size = box.max - box.min;
    draw_rectangle(runtime, box.min, size, lit ? accent_color : panel_color);
    draw_outline(runtime, box.min, size, 2, lit ? bright_color : accent_color);
    const Color mark = lit ? panel_color : bright_color;
    // The play mark: a triangle 16 pixels high, pointing right.
    const Vec2 tip = box.min + Vec2{28, size.y / 2};
    const std::array corners{runtime.camera.screen_to_world(tip + Vec2{-14, -8}),
                             runtime.camera.screen_to_world(tip),
                             runtime.camera.screen_to_world(tip + Vec2{-14, 8})};
    runtime.polygon(corners, mark);
    // RUN at scale 2 (14 pixels high) and the hint at scale 1 beside it, both centered on the
    // mark.
    draw_text(runtime, "RUN", snapped(box.min + Vec2{36, size.y / 2 - 7}), 2, mark);
    // The hint ends 35 pixels inside the full width; a box narrowed for a small viewport
    // shows RUN alone.
    constexpr std::string_view hint = "CLICK OR SPACE";
    if (82 + text_width(hint) <= size.x)
        draw_text(runtime, hint, snapped(box.min + Vec2{82, size.y / 2 - 3.5f}), 1,
                  lit ? panel_color : text_color);
}
} // namespace i4sim
