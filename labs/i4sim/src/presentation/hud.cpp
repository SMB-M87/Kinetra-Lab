#include "i4sim/presentation/detail/hud.h"

#include <kinetra/runtime/hud_text.h>

#include <sstream>
#include <stdexcept>
#include <string>

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
std::string totals_text(const Simulation &simulation) {
    const auto &motion = simulation.motion_settings();
    std::ostringstream text;
    text << "TICK " << simulation.ticks() << " UPS " << simulation.updates_per_second() << " CONTACTS "
         << simulation.collisions() << " DURATION " << simulation.contact_ticks();
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
        hud::draw_line(runtime, totals_text(simulation), 2, hud::statistics_color);
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
} // namespace i4sim
