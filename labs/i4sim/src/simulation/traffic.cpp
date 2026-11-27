#include "i4sim/simulation/detail/traffic.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace i4sim::detail {
using namespace kinetra;
namespace {
/// Extra clearance in millimeters between a retreating mover and the corridor mouth.
constexpr float refuge_margin_mm = 4;
/// Extra length in millimeters of the exit pocket kept free at each corridor mouth.
constexpr float pocket_margin_mm = 6;
/// Retreat costs within this many millimeters count as equal.
constexpr float cost_tolerance_mm = 0.01f;
/// True when the body of `mover` overlaps `corridor` (already grown by the clearance).
bool inside(const Mover &mover, const Body &corridor) {
    return swept_box_overlaps(mover.body.position, mover.body.position, mover.body.half_extents, corridor);
}
/// A box body covering `area`.
Body box_over(Aabb area) {
    Body box;
    box.position = (area.min + area.max) / 2;
    box.half_extents = (area.max - area.min) / 2;
    return box;
}
} // namespace

Traffic::Traffic(const FloorPlan &floor) : zones_(floor.zones().size()) {
}

void Traffic::configure(const FloorPlan &floor, const MotionSettings &settings) {
    for (std::size_t j = 0; j < zones_.size(); ++j)
        if (!(floor.zones()[j].corridor ? settings.corridor_traffic : settings.junction_traffic)) {
            zones_[j].gate.clear();
            zones_[j].exit.reset();
            zones_[j].recovery.reset();
        }
}

void Traffic::begin_tick(Fleet &fleet) {
    fleet.stops.assign(fleet.movers.size(), {});
    fleet.controls.assign(fleet.movers.size(), std::nullopt);
    // Coordination is an output of this tick; passing coordination sets its part too.
    for (auto &m : fleet.movers)
        m.coordination = {};
}

std::optional<Aabb> Traffic::exit_reservation(std::size_t zone) const {
    const auto &exit = zones_[zone].exit;
    return exit ? std::optional(exit->area) : std::nullopt;
}

std::size_t Traffic::recovering() const {
    return std::size_t(std::count_if(zones_.begin(), zones_.end(),
                                     [](const ZoneState &zone) { return zone.recovery.has_value(); }));
}

std::optional<Vec2> Traffic::retreat_target(std::size_t mover) const {
    for (const auto &zone : zones_)
        if (zone.recovery && zone.recovery->yielding == mover)
            return zone.recovery->route.front();
    return std::nullopt;
}

void Traffic::cancel_recovery(Fleet &fleet, std::size_t mover, std::uint64_t tick) {
    for (auto &zone : zones_) {
        if (!zone.recovery)
            continue;
        const auto &recovery = *zone.recovery;
        if (mover != recovery.yielding && mover != recovery.passing)
            continue;
        for (auto id : {recovery.yielding, recovery.passing}) {
            fleet.movers[id].coordination.corridor_retreating = false;
            fleet.movers[id].coordination.corridor_waiting = false;
            if (id < fleet.controls.size())
                fleet.controls[id].reset();
        }
        zone.recovery.reset();
        zone.retry_at = tick + corridor_retry_ticks;
    }
}

void Traffic::recover_corridors(const FloorPlan &floor, Fleet &fleet, std::uint64_t tick,
                                const SpatialGrid &index) {
    for (std::size_t j = floor.junction_count(); j < zones_.size(); ++j) {
        const auto &shape = floor.zones()[j];
        Body corridor;
        corridor.position = shape.center;
        corridor.half_extents = shape.half_extents + clearance;
        if (zones_[j].recovery)
            release_recovery(j, fleet, tick, corridor);
        if (!zones_[j].recovery && tick >= zones_[j].retry_at)
            start_recovery(floor, j, fleet, tick, index, corridor);
        if (zones_[j].recovery)
            steer_recovery(floor, j, fleet);
    }
}

void Traffic::release_recovery(std::size_t j, Fleet &fleet, std::uint64_t tick, const Body &corridor) {
    auto &zone = zones_[j];
    const auto &recovery = *zone.recovery;
    if (fleet.movers[recovery.yielding].moving() && fleet.movers[recovery.passing].moving() &&
        !(recovery.yielded && !inside(fleet.movers[recovery.passing], corridor)))
        return;
    // Both movers start a fresh progress measurement on their own routes.
    for (auto id : {recovery.yielding, recovery.passing}) {
        fleet.progress[id].no_progress = fleet.progress[id].replan = 0;
        fleet.progress[id].best_remaining =
            route_remaining(fleet.movers[id].body.position, fleet.movers[id].route);
    }
    zone.recovery.reset();
    zone.retry_at = tick + corridor_retry_ticks;
}

void Traffic::start_recovery(const FloorPlan &floor, std::size_t j, const Fleet &fleet, std::uint64_t tick,
                             const SpatialGrid &index, const Body &corridor) {
    auto &zone = zones_[j];
    const auto &shape = floor.zones()[j];
    std::vector<std::size_t> occupants;
    for (auto id :
         index.query({corridor.position - corridor.half_extents, corridor.position + corridor.half_extents}))
        if (inside(fleet.movers[id], corridor))
            occupants.push_back(id);
    if (occupants.size() != 2)
        return;
    auto axis = [&](Vec2 value) { return shape.vertical ? value.y : value.x; };
    const auto &a = fleet.movers[occupants[0]], &b = fleet.movers[occupants[1]];
    // Only two stalled movers that stand still and want to pass each other.
    if (a.goal == GoalKind::making_way || b.goal == GoalKind::making_way || a.held || b.held || !a.moving() ||
        !b.moving() || fleet.progress[occupants[0]].no_progress < stall_ticks ||
        fleet.progress[occupants[1]].no_progress < stall_ticks || length(a.body.velocity) > stop_speed ||
        length(b.body.velocity) > stop_speed ||
        axis(a.destination - a.body.position) * axis(b.destination - b.body.position) >= 0)
        return;
    zone.retry_at = tick + corridor_retry_ticks;
    float best = std::numeric_limits<float>::infinity();
    for (auto id : occupants) {
        bool claimed = false;
        for (std::size_t k = 0; k < zones_.size(); ++k)
            if (k != j && zones_[k].recovery &&
                (zones_[k].recovery->yielding == id || zones_[k].recovery->passing == id))
                claimed = true;
        if (claimed)
            continue;
        const auto &m = fleet.movers[id];
        const float backwards = axis(m.destination - m.body.position) > 0 ? -1.f : 1.f;
        // The refuge lies just behind the corridor mouth, one cell to either side.
        for (float side : {-1.f, 1.f}) {
            Vec2 refuge = shape.center;
            if (shape.vertical) {
                refuge.y +=
                    backwards * (shape.half_extents.y +
                                 std::max(floor.cell_size().y / 2, m.body.half_extents.y + refuge_margin_mm));
                refuge.x += side * floor.cell_size().x;
            } else {
                refuge.x +=
                    backwards * (shape.half_extents.x +
                                 std::max(floor.cell_size().x / 2, m.body.half_extents.x + refuge_margin_mm));
                refuge.y += side * floor.cell_size().y;
            }
            auto route = floor.grid().path(m.body.position, refuge, m.body.half_extents + clearance);
            if (route.empty())
                continue;
            const bool clear = refuge_clear(j, id, route, fleet, index);
            const float cost = route_remaining(m.body.position, route);
            // The shorter retreat yields; on a tie the higher mover index does.
            if (clear &&
                (cost < best - cost_tolerance_mm || (std::abs(cost - best) <= cost_tolerance_mm &&
                                                     zone.recovery && id > zone.recovery->yielding))) {
                const auto passing = id == occupants[0] ? occupants[1] : occupants[0];
                zone.recovery = CorridorRecovery{
                    id, passing, refuge, fleet.movers[passing].body.position, std::move(route), false};
                best = cost;
            }
        }
    }
    if (zone.recovery)
        zone.exit.reset();
}

bool Traffic::refuge_clear(std::size_t j, std::size_t mover, const std::vector<Vec2> &route,
                           const Fleet &fleet, const SpatialGrid &index) const {
    const auto &m = fleet.movers[mover];
    bool clear = true;
    Vec2 from = m.body.position;
    for (auto to : route) {
        const auto extent = m.body.half_extents + clearance;
        const Aabb area{{std::min(from.x, to.x) - extent.x, std::min(from.y, to.y) - extent.y},
                        {std::max(from.x, to.x) + extent.x, std::max(from.y, to.y) + extent.y}};
        for (auto other : index.query(area))
            if (other != mover && swept_box_overlaps(from, to, extent, fleet.movers[other].body))
                clear = false;
        for (std::size_t k = 0; k < zones_.size(); ++k) {
            if (k == j || !zones_[k].exit)
                continue;
            if (swept_box_overlaps(from, to, extent, box_over(zones_[k].exit->area)))
                clear = false;
        }
        from = to;
        if (!clear)
            break;
    }
    return clear;
}

void Traffic::steer_recovery(const FloorPlan &floor, std::size_t j, Fleet &fleet) {
    auto &recovery = *zones_[j].recovery;
    auto &yielding = fleet.movers[recovery.yielding];
    while (recovery.route.size() > 1 &&
           distance(yielding.body.position, recovery.route.front()) < waypoint_reach_mm &&
           floor.grid().clear(yielding.body.position, recovery.route[1], yielding.body.half_extents))
        recovery.route.erase(recovery.route.begin());
    recovery.yielded =
        recovery.yielded || (distance(yielding.body.position, recovery.refuge) < arrival_distance_mm &&
                             length(yielding.body.velocity) < stop_speed);
    fleet.controls[recovery.yielding] = TrafficControl{j, recovery.route.front(), recovery.route.size() == 1};
    yielding.coordination.corridor_retreating = true;
    if (recovery.yielded)
        yielding.coordination.corridor_waiting = true;
    else {
        fleet.controls[recovery.passing] = TrafficControl{j, recovery.hold, true};
        fleet.movers[recovery.passing].coordination.corridor_waiting = true;
    }
}

void Traffic::admit(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                    const SpatialGrid &index, std::span<const Aabb> bodies) {
    if (!motion.junction_traffic && !motion.corridor_traffic)
        return;
    const float reach =
        2 * std::max(floor.cell_size().x, floor.cell_size().y) + braking_distance(motion.speed, motion.force);
    const auto requests = find_requests(floor, fleet, motion, reach);
    // Release stale reservations before considering any new grant. Existing grants
    // keep their exit even after a goal change, until request and occupancy clear.
    for (std::size_t j = 0; j < zones_.size(); ++j) {
        auto &exit = zones_[j].exit;
        const auto &waiting = requests.requests[j], &present = requests.occupants[j];
        if (exit && std::find(waiting.begin(), waiting.end(), exit->agent) == waiting.end() &&
            std::find(present.begin(), present.end(), exit->agent) == present.end())
            exit.reset();
    }
    for (std::size_t j = 0; j < zones_.size(); ++j)
        admit_zone(floor, j, requests, fleet, motion, index, bodies);
    protect_exits(requests, fleet, motion, index, bodies, reach, floor);
}

Traffic::Requests Traffic::find_requests(const FloorPlan &floor, const Fleet &fleet,
                                         const MotionSettings &motion, float reach) const {
    Requests result;
    result.requests.resize(zones_.size());
    result.occupants.resize(zones_.size());
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        const auto &m = fleet.movers[i];
        float nearest = std::numeric_limits<float>::infinity();
        std::optional<std::size_t> next;
        const auto range = Vec2{reach, reach} + m.body.half_extents;
        for (auto j : floor.zone_index().query({m.body.position - range, m.body.position + range})) {
            const auto &shape = floor.zones()[j];
            if (!(shape.corridor ? motion.corridor_traffic : motion.junction_traffic))
                continue;
            const Vec2 half = shape.half_extents + clearance;
            const auto offset = m.body.position - shape.center;
            const auto extent = m.body.half_extents + half;
            if (std::abs(offset.x) < extent.x && std::abs(offset.y) < extent.y)
                result.occupants[j].push_back(i);
            // A temporary parking trip can leave a zone enabled around it, but
            // cannot request entry or reserve the exit it is already avoiding.
            if (m.goal == GoalKind::making_way || m.held || !m.moving() || m.route.empty())
                continue;
            Body zone;
            zone.position = shape.center;
            zone.half_extents = half;
            // A new goal may cancel a route while the mover is still coasting
            // toward the zone. Retain the request until braking can clear it.
            const float braking_time = length(m.body.velocity) / motion.force + 1;
            if (swept_box_overlaps(m.body.position, m.body.position + m.body.velocity * braking_time,
                                   m.body.half_extents, zone)) {
                if (nearest > 0) {
                    nearest = 0;
                    next = j;
                }
                continue;
            }
            float travelled = 0;
            Vec2 from = m.body.position;
            for (auto to : m.route) {
                const float segment = distance(from, to);
                to = from + limited(to - from, std::max(0.f, reach - travelled));
                if (swept_box_overlaps(from, to, m.body.half_extents, zone)) {
                    // Junctions have disjoint cells; route distance orders requests.
                    const float along =
                        travelled +
                        std::clamp(dot(zone.position - from, normalized(to - from)), 0.f, distance(from, to));
                    if (along < nearest) {
                        nearest = along;
                        next = j;
                    }
                    break;
                }
                travelled += segment;
                if (travelled >= reach)
                    break;
                from = to;
            }
        }
        if (next)
            result.requests[*next].push_back(i);
    }
    result.occupied.resize(fleet.movers.size());
    for (const auto &present : result.occupants)
        for (auto id : present)
            result.occupied[id] = true;
    return result;
}

void Traffic::admit_zone(const FloorPlan &floor, std::size_t j, const Requests &requests, Fleet &fleet,
                         const MotionSettings &motion, const SpatialGrid &index,
                         std::span<const Aabb> bodies) {
    auto &zone = zones_[j];
    const auto &shape = floor.zones()[j];
    const auto &waiting = requests.requests[j];
    std::vector<std::uint64_t> blocked;
    std::vector<std::optional<Aabb>> exits(waiting.size());
    for (std::size_t n = 0; n < waiting.size(); ++n) {
        const auto id = waiting[n];
        if (zone.exit && zone.exit->agent == id)
            continue;
        exits[n] = exit_space(floor, fleet.movers[id], shape, motion);
        if (!exits[n] || !exit_free(j, std::size_t(id), *exits[n], fleet, motion, index, bodies))
            blocked.push_back(id);
    }
    const auto admission = zone.gate.update(waiting, requests.occupants[j], blocked);
    if (zone.exit && admission.agent != zone.exit->agent)
        zone.exit.reset();
    for (std::size_t n = 0; n < waiting.size(); ++n) {
        const auto id = waiting[n];
        if (admission.agent == id && !zone.exit && exits[n] &&
            std::find(blocked.begin(), blocked.end(), id) == blocked.end())
            zone.exit = ExitReservation{id, *exits[n]};
        if (admission.agent == id || requests.occupied[id] ||
            (fleet.controls[id] && fleet.controls[id]->zone == j))
            continue;
        auto half = shape.half_extents + clearance;
        if (shape.corridor) {
            // Leave a body-length exit pocket free at either mouth, even
            // when a parked occupant has no route indicating its exit yet.
            const float braking = braking_distance(motion.speed, motion.force) + pocket_margin_mm;
            if (!shape.vertical)
                half.x += 2 * fleet.max_half.x + braking;
            else
                half.y += 2 * fleet.max_half.y + braking;
        }
        auto &mover = fleet.movers[id];
        fleet.stops[id].push_back(stop_before(mover.body, {shape.center - half, shape.center + half}));
        if (shape.corridor)
            mover.coordination.corridor_waiting = true;
        else
            mover.coordination.junction_waiting = true;
        mover.coordination.exit_waiting = std::find(blocked.begin(), blocked.end(), id) != blocked.end();
    }
}

bool Traffic::exit_free(std::size_t j, std::size_t mover, Aabb exit, const Fleet &fleet,
                        const MotionSettings &motion, const SpatialGrid &index,
                        std::span<const Aabb> bodies) const {
    const auto pocket = box_over(exit);
    const float stopping = motion.speed * (motion.speed / motion.force + 1);
    const Vec2 padding{stopping, stopping};
    bool available = true;
    // No other mover may stand in the pocket or coast into it while braking.
    for (auto other : index.query({exit.min - padding, exit.max + padding})) {
        if (other == mover)
            continue;
        const auto &moving = fleet.movers[other];
        const auto velocity = moving.moving() && !moving.held ? moving.body.velocity : Vec2{};
        const auto coast = moving.body.position + velocity * (length(velocity) / motion.force + 1);
        if (bodies[other].overlaps(exit) ||
            swept_box_overlaps(moving.body.position, coast, moving.body.half_extents, pocket))
            available = false;
    }
    for (std::size_t k = 0; k < zones_.size(); ++k)
        if (k != j && zones_[k].exit && zones_[k].exit->area.overlaps(exit))
            available = false;
    return available;
}

void Traffic::protect_exits(const Requests &requests, Fleet &fleet, const MotionSettings &motion,
                            const SpatialGrid &index, std::span<const Aabb> bodies, float reach,
                            const FloorPlan &floor) const {
    // Keep approaching traffic out of an admitted mover's exit pocket. Agents
    // already inside a pocket or junction may always leave; never trap an occupant.
    for (std::size_t j = 0; j < zones_.size(); ++j) {
        if (!zones_[j].exit)
            continue;
        const auto &exit = *zones_[j].exit;
        const auto pocket = box_over(exit.area);
        for (auto id :
             index.query({exit.area.min - Vec2{reach, reach}, exit.area.max + Vec2{reach, reach}})) {
            auto &m = fleet.movers[id];
            if ((fleet.controls[id] && fleet.controls[id]->zone == j) || id == exit.agent ||
                requests.occupied[id] || m.held || !m.moving() || m.route.empty() ||
                bodies[id].overlaps(exit.area))
                continue;
            const auto coast =
                m.body.position + m.body.velocity * (length(m.body.velocity) / motion.force + 1);
            const auto target = m.body.position + limited(m.route.front() - m.body.position, reach);
            if (!swept_box_overlaps(m.body.position, target, m.body.half_extents, pocket) &&
                !swept_box_overlaps(m.body.position, coast, m.body.half_extents, pocket))
                continue;
            fleet.stops[id].push_back(stop_before(m.body, exit.area));
            if (floor.zones()[j].corridor)
                m.coordination.corridor_waiting = true;
            else
                m.coordination.junction_waiting = true;
            m.coordination.exit_waiting = true;
        }
    }
}

std::optional<Aabb> Traffic::exit_space(const FloorPlan &floor, const Mover &m, const ZoneGeometry &shape,
                                        const MotionSettings &motion) const {
    Body zone;
    zone.position = shape.center;
    zone.half_extents = shape.half_extents + clearance;
    const Vec2 extent = zone.half_extents + m.body.half_extents;
    const Vec2 low = zone.position - extent, high = zone.position + extent;
    bool entered = false;
    Vec2 from = m.body.position;
    for (auto to : m.route) {
        entered = entered || swept_box_overlaps(from, to, m.body.half_extents, zone);
        const bool outside = to.x < low.x || to.x > high.x || to.y < low.y || to.y > high.y;
        if (entered && outside) {
            // Find where the whole body first clears the Minkowski-expanded zone,
            // then reserve enough space to brake outside, capped by the route goal.
            const auto travel = to - from;
            float leave = 1;
            if (travel.x > 0)
                leave = std::min(leave, (high.x - from.x) / travel.x);
            if (travel.x < 0)
                leave = std::min(leave, (low.x - from.x) / travel.x);
            if (travel.y > 0)
                leave = std::min(leave, (high.y - from.y) / travel.y);
            if (travel.y < 0)
                leave = std::min(leave, (low.y - from.y) / travel.y);
            const auto cleared = from + travel * std::clamp(leave, 0.f, 1.f);
            const float braking_space = braking_distance(motion.speed, motion.force) + clearance_mm;
            const auto end = cleared + limited(to - cleared, braking_space);
            const auto footprint = m.body.half_extents + clearance;
            if (!floor.grid().clear(cleared, end, footprint))
                return std::nullopt;
            return Aabb{{std::min(cleared.x, end.x) - footprint.x, std::min(cleared.y, end.y) - footprint.y},
                        {std::max(cleared.x, end.x) + footprint.x, std::max(cleared.y, end.y) + footprint.y}};
        }
        from = to;
    }
    return std::nullopt;
}
} // namespace i4sim::detail
