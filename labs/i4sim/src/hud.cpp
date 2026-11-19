#include "hud.h"

#include <kinetra/bitmap_text.h>

#include <sstream>
#include <string>

namespace i4sim {
namespace {
using namespace kinetra;

constexpr Color hud_color{1, 1, 0};
constexpr float hud_scale = 1.5f;
/// Height of one HUD line in screen pixels, at hud_scale.
constexpr float line_height = 20;

/// Draws `text` on HUD line `line`, counted from the bottom of the viewport (1 is lowest).
void draw_line(Runtime &runtime, std::string_view text, int line, Color color = hud_color) {
    draw_text(runtime, text, {10, runtime.camera.viewport.y - float(line) * line_height}, hud_scale, color);
}

/// HUD text for a selected mover's status.
const char *status_text(MoverStatus status) {
    switch (status) {
    case MoverStatus::held:
        return "BLOCKED";
    case MoverStatus::corridor_retreat:
        return "CORRIDOR RETREAT";
    case MoverStatus::passing_detour:
        return "PASSING DETOUR";
    case MoverStatus::unreachable:
        return "UNREACHABLE";
    case MoverStatus::allocated:
        return "ALLOCATED";
    case MoverStatus::available:
        return "IDLE - SHIFT CLICK GOAL";
    }
    return "";
}

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

void draw_mover_inspection(Runtime &runtime, const Simulation &simulation, std::size_t index) {
    const auto &mover = simulation.movers().at(index);
    std::ostringstream summary;
    summary << mover.definition.id << " | DISTANCE " << int(mover.distance) << " MM | CONTACT TICKS "
            << mover.collision_ticks << " | " << status_text(mover_status(mover));
    draw_line(runtime, summary.str(), 4);
    std::ostringstream details;
    details << "SPEED " << length(mover.body.velocity) << " FORCE " << length(mover.body.acceleration)
            << " GOAL " << int(mover.destination.x) << "," << int(mover.destination.y)
            << (simulation.stalled(index) ? " STALLED" : "");
    draw_line(runtime, details.str(), 3);
}

void draw_producer_inspection(Runtime &runtime, const Simulation &simulation, std::size_t index) {
    const auto &producer = simulation.producers().at(index);
    draw_line(runtime,
              producer.definition.id + " | QUEUE " + std::to_string(producer.queue.size()) + " | REMAINING " +
                  std::to_string(producer.remaining) + " | TASKS " + std::to_string(producer.interactions) +
                  (producer.blocked ? " | BLOCKED" : ""),
              3);
}
} // namespace

void draw_hud(Runtime &runtime, const Simulation &simulation, const HudContent &content) {
    if (content.statistics) {
        draw_line(runtime, totals_text(simulation), 2);
        draw_line(runtime, motion_text(simulation), 1);
    }
    if (content.selected_mover)
        draw_mover_inspection(runtime, simulation, *content.selected_mover);
    if (content.selected_producer)
        draw_producer_inspection(runtime, simulation, *content.selected_producer);
    if (!content.description.empty())
        draw_line(runtime, content.description, 5, {0.75f, 0.85f, 1});
    // Below the SETTINGS and DEBUG toolbar, smaller than the HUD lines.
    if (!content.message.empty())
        draw_text(runtime, content.message, {12, 98}, 0.8f, {1, 0.4f, 0.3f});
}
} // namespace i4sim
