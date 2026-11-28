#include "i4sim/simulation/detail/stall_detector.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace i4sim::detail {
using namespace kinetra;
namespace {
/// A larger excursion than the small adjustments of stationary steering, in millimeters.
constexpr float excursion_distance_mm = 2;
constexpr unsigned oscillation_reversals = 2;
/// Legs at more than about 143 degrees oppose each other.
constexpr float opposing_cosine = -0.8f;
/// A pushing neighbor is within 1.6 times the combined half diagonals: in contact or about
/// to be, at the factory's clearances.
constexpr float pushing_reach = 1.6f;
/// A pushing neighbor lies within about 78 degrees of the mover's leg.
constexpr float ahead_cosine = 0.2f;

/// A mover on its own route, outside any coordination, is observed; one that stands still
/// is a stationary blocker to others; one under any other control moves without being
/// observed.
StallMoverState state(const Mover &mover) {
    if (!mover.moving() || mover.held)
        return StallMoverState::standing;
    const auto &coord = mover.coordination;
    if (mover.goal == GoalKind::making_way || mover.route.empty() || coord.junction_waiting ||
        coord.exit_waiting || coord.corridor_waiting || coord.corridor_retreating || coord.passing)
        return StallMoverState::busy;
    return StallMoverState::observed;
}

} // namespace

StallObservation stall_observation(const MotionSettings &motion) {
    return {.threshold_steps = stall_ticks,
            .excursion = excursion_distance_mm,
            .oscillation_reversals = oscillation_reversals,
            // A tiny force accepted by configure_motion can make the braking distance
            // infinite; the whole leg is then within reach, as it was before the engine.
            .lookahead =
                std::min(motion.speed * motion.horizon + braking_distance(motion.speed, motion.force),
                         std::numeric_limits<float>::max()),
            .clearance = clearance,
            .stop_speed = stop_speed,
            .opposing_cosine = opposing_cosine,
            .pushing_reach = pushing_reach,
            .ahead_cosine = ahead_cosine};
}

void StallDetector::observe(const Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                            EventLog &log) {
    auto &movers = observed_;
    movers.clear();
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        const auto &m = fleet.movers[i];
        movers.push_back({m.body, m.route.empty() ? m.body.position : m.route.front(), m.destination,
                          fleet.progress[i].no_progress, state(m)});
    }
    for (const auto &event : detector_.observe(movers, stall_observation(motion), tick))
        if (log.recording())
            log.add({.tick = tick,
                     .kind = event.kind == StallEvent::Kind::started ? EventKind::stall_started
                                                                     : EventKind::stall_ended,
                     .movers = {event.mover, 0},
                     .stall = event.diagnosis});
}

std::optional<StallDetails> StallDetector::details(std::size_t mover) const {
    if (mover >= movers_)
        throw std::out_of_range("Unknown mover");
    return detector_.episode(mover);
}
} // namespace i4sim::detail
