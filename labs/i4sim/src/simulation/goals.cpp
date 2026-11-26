#include "i4sim/simulation/detail/goals.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/routing.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <algorithm>
#include <limits>

namespace i4sim::detail {
using namespace kinetra;
void Goals::set(Mover &m, Vec2 target) {
    traffic.cancel_recovery(fleet, fleet.index(m), tick);
    passing.cancel(floor, fleet, motion, fleet.index(m), tick, log);
    // A manual goal stays manual until arrival; a parking staging goal is replaced.
    if (m.goal == GoalKind::parking_staging)
        m.goal = GoalKind::automatic;
    m.destination = target;
    m.route = plan_route(floor, fleet, motion, fleet.index(m), target);
    m.lifecycle = m.route.empty() ? MoverLifecycle::unreachable : MoverLifecycle::moving;
    auto &tracking = fleet.progress[fleet.index(m)];
    tracking.replan = tracking.no_progress = 0;
    tracking.best_remaining = route_remaining(m.body.position, m.route);
    m.detour = false;
    tracking.recovery_attempts = 0;
}
Vec2 Goals::holding_position(const Mover &m, Vec2 target) const {
    // Each mover retains its own blueprint parking reservation. Prefer that
    // known safe waiting area over filling a narrow factory aisle with queues.
    if (floor.grid().cell(m.parking) != floor.grid().cell(target) &&
        distance(m.parking, target) > length(m.body.half_extents) * 2 &&
        (!motion.parking_traffic || !parking_conflict(m, m.parking)) &&
        !floor.grid().path(m.body.position, m.parking, m.body.half_extents).empty())
        return m.parking;
    float best = std::numeric_limits<float>::infinity();
    Vec2 result = m.parking;
    for (int id = 0; id < floor.grid().width() * floor.grid().height(); ++id) {
        if (floor.grid().blocked(id))
            continue;
        const auto candidate = floor.grid().center(id);
        bool available = !floor.processing_cell(id);
        for (const auto &other : fleet.movers)
            if (&other != &m &&
                (distance(candidate, other.body.position) <
                     length(m.body.half_extents + other.body.half_extents) + 10 ||
                 (other.moving() && distance(candidate, other.destination) <
                                        length(m.body.half_extents + other.body.half_extents) + 10)))
                available = false;
        if (!available || (motion.parking_traffic && parking_conflict(m, candidate)))
            continue;
        const float score = distance(candidate, target) +
                            floor.grid().weight(id) * std::max(floor.cell_size().x, floor.cell_size().y);
        if (score < best && !floor.grid().path(m.body.position, candidate, m.body.half_extents).empty()) {
            best = score;
            result = candidate;
        }
    }
    return result;
}
bool Goals::parking_conflict(const Mover &m, Vec2 position) const {
    Body parked = m.body;
    parked.position = position;
    for (const auto &other : fleet.movers) {
        if (&other == &m || other.held || !other.moving() || other.goal == GoalKind::parking_staging ||
            (other.goal != GoalKind::manual && distance(other.destination, other.parking) < same_point_mm))
            continue;
        Vec2 from = other.body.position;
        for (const auto to : other.route) {
            if (swept_box_overlaps(from, to, other.body.half_extents + clearance, parked))
                return true;
            from = to;
        }
    }
    return false;
}
void Goals::coordinate_parking() {
    if (!motion.parking_traffic)
        return;
    for (auto &m : fleet.movers) {
        const bool staging = m.goal == GoalKind::parking_staging;
        if (m.held || m.goal == GoalKind::manual ||
            (!staging && (!m.moving() || distance(m.destination, m.parking) >= same_point_mm)))
            continue;
        if (!parking_conflict(m, m.parking)) {
            if (staging)
                set(m, m.parking);
            continue;
        }
        if (staging)
            continue;
        // Reuse footprint-aware staging selection, excluding protected routes.
        // If no staging place is reachable, retain the existing goal and let
        // local avoidance stop safely; retry the interlock on the next tick.
        const auto staging_place = holding_position(m, m.parking);
        if (distance(staging_place, m.parking) > same_point_mm) {
            set(m, staging_place);
            m.goal = GoalKind::parking_staging;
        }
    }
}
} // namespace i4sim::detail
