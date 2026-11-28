#include "i4sim/simulation/detail/motion_step.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/make_way.h"
#include "i4sim/simulation/detail/routing.h"
#include "i4sim/simulation/detail/simulation_constants.h"
#include "i4sim/simulation/detail/stall_detector.h"

#include <kinetra/simulation/steering.h>

#include <algorithm>
#include <cmath>

namespace i4sim::detail {
using namespace kinetra;
namespace {
/// Margin in millimeters beyond the steering reach within which borders are considered.
constexpr float border_search_margin_mm = 10;
/// SAFETY STOP keeps a mover this far, in millimeters, from every neighbor: more than the
/// rounding of a position on the largest floor, so a stopped pair never counts as touching.
constexpr float safety_gap_mm = 0.05f;
/// The same for a border, which a mover may touch, as at a parking place against a wall, but
/// not cross: twice the rounding of a position on the largest floor.
constexpr float border_gap_mm = 0.001f;
/// A cut of at most this many millimeters is the end of an approach that steering already
/// slows, not a stop: SAFETY STOP applies it but does not count it.
constexpr float negligible_cut_mm = 0.01f;

/// Distance in millimeters within which a box counts as flush with an obstacle on one axis
/// although rounding put it slightly inside: it slides along without a stop.
constexpr float flush_mm = 0.001f;

/// The part of `move` that keeps a box at `offset` from an obstacle out of touch with it,
/// for `extent` the half extents of both combined. On each axis the box may come no closer
/// than `gap`, or than it is now when it is closer already. Where the move would
/// bring it that close on both axes at once, the component on the axis it reaches last is cut
/// at that point and the other is kept, so the box slides along the obstacle. A box that
/// already overlaps on both, as after a contact, only stops approaching on the axis it
/// overlaps least.
Vec2 clear_move(Vec2 offset, Vec2 move, Vec2 extent, float gap) {
    Vec2 limit;
    for (int axis = 0; axis < 2; ++axis) {
        const float distance = std::abs(axis == 0 ? offset.x : offset.y);
        const float touch = axis == 0 ? extent.x : extent.y;
        // The gap counts, unless the box is already closer on this axis, flush or nearly so:
        // then it may only come no closer. A box that overlaps on this axis, as one beside the
        // obstacle does, keeps the gap.
        float bound = touch + gap;
        if (distance < bound && distance >= touch - flush_mm)
            bound = distance;
        (axis == 0 ? limit.x : limit.y) = bound;
    }
    const bool inside_x = std::abs(offset.x) < limit.x, inside_y = std::abs(offset.y) < limit.y;
    if (inside_x && inside_y) {
        const bool x_axis = std::abs(offset.x) - extent.x >= std::abs(offset.y) - extent.y;
        float &along = x_axis ? move.x : move.y;
        if ((x_axis ? offset.x : offset.y) * along < 0)
            along = 0;
        return move;
    }
    // Slab test of the move against the region: the share of the move before it enters, and
    // the axis on which it enters last.
    float enter = 0, leave = 1;
    int entering = -1;
    for (int axis = 0; axis < 2; ++axis) {
        const float start = axis == 0 ? offset.x : offset.y, step = axis == 0 ? move.x : move.y;
        const float half = axis == 0 ? limit.x : limit.y;
        if (step == 0) {
            if (std::abs(start) >= half)
                return move;
            continue;
        }
        float first = (-half - start) / step, last = (half - start) / step;
        if (first > last)
            std::swap(first, last);
        if (first >= enter) {
            enter = first;
            entering = axis;
        }
        leave = std::min(leave, last);
    }
    if (enter >= leave || entering < 0)
        return move;
    (entering == 0 ? move.x : move.y) *= enter;
    return move;
}
/// Ticks a pushed-back mover keeps the right of way: four stall windows, the best of the
/// windows measured under the prototype's return rule (design: stall remedies in the engine).
constexpr std::uint64_t right_of_way_ticks = 4 * stall_ticks;

/// What a mover is to RIGHT OF WAY at the start of a tick, from the coordination of the last
/// tick: on its own route, standing, or under another control (a traffic control, a wait at a
/// junction or corridor, a corridor retreat, a passing plan, a make-way trip). The first tick
/// has no controls yet.
StallMoverState right_of_way_state(const Fleet &fleet, std::size_t id) {
    const auto &m = fleet.movers[id];
    if (!m.moving() || m.held)
        return StallMoverState::standing;
    const auto &coord = m.coordination;
    if (m.goal == GoalKind::making_way || (id < fleet.controls.size() && fleet.controls[id]) ||
        m.route.empty() || coord.junction_waiting || coord.exit_waiting || coord.corridor_waiting ||
        coord.corridor_retreating || coord.passing)
        return StallMoverState::busy;
    return StallMoverState::observed;
}
} // namespace

void MotionStep::end_right_of_way(Fleet &fleet, std::uint64_t tick, EventLog &log) {
    for (std::size_t i = 0; i < fleet.movers.size(); ++i)
        if (right_of_way_.granted(i)) {
            fleet.movers[i].coordination.right_of_way = false;
            if (log.recording())
                log.add({.tick = tick, .kind = EventKind::right_of_way_ended, .movers = {i, 0}});
        }
    right_of_way_.clear();
}

void MotionStep::update_right_of_way(Goals &goals) {
    const auto &fleet = goals.fleet;
    auto record = [&](EventKind kind, std::size_t mover, std::size_t pusher) {
        if (goals.log.recording())
            goals.log.add({.tick = goals.tick, .kind = kind, .movers = {mover, pusher}});
    };
    if (!goals.motion.right_of_way || !supports_right_of_way(goals.motion.profile)) {
        end_right_of_way(goals.fleet, goals.tick, goals.log);
        return;
    }
    auto &movers = stall_movers_;
    movers.clear();
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        const auto &m = fleet.movers[i];
        movers.push_back({m.body, m.route.empty() ? m.body.position : m.route.front(), m.destination,
                          fleet.progress[i].no_progress, right_of_way_state(fleet, i)});
    }
    for (const auto &event :
         right_of_way_.step(movers, stall_observation(goals.motion), {right_of_way_ticks}, goals.tick))
        record(event.kind == RightOfWayEvent::Kind::granted ? EventKind::right_of_way_granted
                                                            : EventKind::right_of_way_ended,
               event.mover, event.pusher.value_or(0));
}

void MotionStep::run(Goals &goals, Production &production) {
    auto &fleet = goals.fleet;
    const auto &motion = goals.motion;
    // With coordination on, every route is prepared before coordination reads them.
    const bool prepare_together = (motion.junction_traffic && goals.floor.junction_count() > 0) ||
                                  (motion.corridor_traffic && goals.floor.corridor_count() > 0) ||
                                  (PredictivePassing::enabled(motion) && fleet.movers.size() > 1);
    // Decided from the state the last tick left, before any route preparation of this tick.
    update_right_of_way(goals);
    if (prepare_together)
        for (std::size_t i = 0; i < fleet.movers.size(); ++i)
            follow_route(goals, i);
    avoidance_limited_ = 0;
    auto &snapshot = snapshot_;
    auto &boxes = boxes_;
    snapshot.clear();
    boxes.clear();
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        const auto &m = fleet.movers[i];
        snapshot.push_back(m.body);
        if (m.held || !m.moving()) {
            snapshot.back().motion = Motion::fixed;
            snapshot.back().velocity = {};
        } else if (right_of_way_.granted(i)) {
            // The others stop counting on it to make room: Reciprocal's agents take the whole
            // avoidance instead of half, and Improved's steering avoids it as a standing body.
            // It still avoids them itself.
            snapshot.back().motion = Motion::fixed;
        }
        boxes.push_back(box_of(i, m.body));
    }
    // The floor, and so its cell size, belongs to the simulation; the check keeps the index
    // right should a step ever move to another floor.
    const float cell = std::max(goals.floor.cell_size().x, goals.floor.cell_size().y);
    if (!index_ || index_cell_ != cell) {
        index_.emplace(cell);
        index_cell_ = cell;
    }
    auto &index = *index_;
    index.rebuild(boxes);
    // Corridor recovery and passing claim movers first; admission then skips their zones.
    Traffic::begin_tick(fleet);
    for (std::size_t i = 0; i < fleet.movers.size(); ++i)
        fleet.movers[i].coordination.right_of_way = right_of_way_.granted(i);
    if (motion.corridor_traffic)
        goals.traffic.recover_corridors(goals.floor, fleet, goals.tick, index);
    goals.passing.coordinate(goals.floor, fleet, motion, goals.tick, goals.log);
    goals.traffic.admit(goals.floor, fleet, motion, index, boxes);
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        auto &m = fleet.movers[i];
        if (!m.product)
            ++m.idle_ticks;
        if (m.held || !m.moving())
            continue;
        if (!prepare_together && !follow_route(goals, i))
            continue;
        surroundings(goals, i, snapshot, index, around_, ids_);
        const auto &around = around_;
        const auto &control = fleet.controls[i];
        // Admission runs after route preparation and can reserve a new exit.
        const bool make_way_stop =
            m.goal == GoalKind::making_way && !MakeWay::traffic_clear(goals, m, m.route);
        const auto target = make_way_stop ? m.body.position : control ? control->target : m.route.front();
        const bool arrival = make_way_stop || (control ? control->arrival : m.route.size() == 1);
        choose_acceleration(goals, m, snapshot[i], around, target, arrival);
        integrate(goals, production, i, around, make_way_stop || control.has_value());
    }
    resolve_contacts(goals);
}

bool MotionStep::follow_route(Goals &goals, std::size_t i) {
    auto &m = goals.fleet.movers[i];
    auto &tracking = goals.fleet.progress[i];
    if (m.held || !m.moving())
        return false;
    // Congestion includes this mover's footprint; it is not evidence that
    // its route is obstructed. Replan only after sustained lack of progress.
    const bool temporary = m.goal == GoalKind::making_way;
    if (!temporary && !m.detour && ++tracking.replan >= replan_ticks &&
        tracking.no_progress >= obstruction_ticks) {
        auto route = plan_route(goals.floor, goals.fleet, goals.motion, i, m.destination);
        if (!route.empty()) {
            m.route = std::move(route);
            tracking.best_remaining = route_remaining(m.body.position, m.route);
        }
        tracking.replan = 0;
    }
    if (m.route.empty()) {
        goals.set(m, m.destination);
        if (m.lifecycle == MoverLifecycle::unreachable)
            return false;
    }
    if (++tracking.no_progress >= stall_ticks + std::min(recovery_backoff_limit, tracking.recovery_attempts) *
                                                    recovery_backoff_ticks &&
        goals.motion.recovery && !temporary) {
        recover_route(goals.floor, goals.fleet, i);
        ++tracking.recovery_attempts;
    }
    while (m.route.size() > 1 && distance(m.body.position, m.route.front()) < waypoint_reach_mm &&
           goals.floor.grid().clear(m.body.position, m.route[1], m.body.half_extents) &&
           (!temporary || MakeWay::shortcut_clear(goals, m, std::span(m.route).subspan(1))))
        m.route.erase(m.route.begin());
    return true;
}

void MotionStep::surroundings(const Goals &goals, std::size_t i, const std::vector<Body> &snapshot,
                              const SpatialGrid &index, Surroundings &around, std::vector<std::size_t> &ids) {
    const auto &fleet = goals.fleet;
    const auto &motion = goals.motion;
    const auto &m = fleet.movers[i];
    around.neighbors.clear();
    around.neighbor_ids.clear();
    around.agents.clear();
    around.edges.clear();
    around.border_ids.clear();
    const float reach =
        length(m.body.half_extents) + goals.floor.mover_max_extent() * 2 + 2 * motion.speed * motion.horizon;
    index.query({m.body.position - Vec2{reach, reach}, m.body.position + Vec2{reach, reach}}, ids);
    for (auto id : ids)
        if (id != i) {
            around.neighbors.push_back(snapshot[id]);
            around.neighbor_ids.push_back(id);
            if (motion.profile == SteeringProfile::reciprocal) {
                // Movers stopped by traffic do not cooperate: the others avoid them alone.
                const auto &other = snapshot[id];
                around.agents.push_back({id, other.position, other.velocity, bounding_radius(other),
                                         other.motion != Motion::fixed &&
                                             !fleet.movers[id].coordination.junction_waiting &&
                                             !fleet.movers[id].coordination.corridor_waiting,
                                         other.half_extents});
            }
        }
    // A border within `limit` of the mover has its bounding box within `limit` too, so the
    // index skips only borders the distance test would reject, and returns the others in
    // ascending order, as the loop over every border did.
    const float limit = length(m.body.half_extents) + motion.speed * motion.horizon + border_search_margin_mm;
    const auto borders = goals.floor.borders();
    goals.floor.border_index().query(
        {m.body.position - Vec2{limit, limit}, m.body.position + Vec2{limit, limit}}, ids);
    for (auto id : ids) {
        const auto &edge = borders[id];
        if (distance_to_segment(m.body.position, edge.start, edge.end) <= limit) {
            around.edges.push_back(edge);
            around.border_ids.push_back(id);
        }
    }
    if (i < fleet.stops.size())
        around.edges.insert(around.edges.end(), fleet.stops[i].begin(), fleet.stops[i].end());
}

void MotionStep::choose_acceleration(const Goals &goals, Mover &m, const Body &start,
                                     const Surroundings &around, Vec2 target, bool arrival) {
    const auto &motion = goals.motion;
    const PrioritySteering settings{
        .max_speed = motion.speed,
        .max_acceleration = motion.force,
        .arrival_radius = length(m.body.half_extents * 2) * 0.45f,
        .border_distance = 2,
        .border_strength = 10,
        .border_lookahead = 2,
        .prediction_steps = int(std::ceil(motion.horizon)),
        .prediction_step = 1,
        .safety_multiplier = 1.025f,
    };
    m.forces = priority_steer(m.body, target, around.neighbors, around.edges, settings, arrival);
    if (motion.profile == SteeringProfile::original)
        return;
    Steering recovery;
    recovery.max_speed = motion.speed;
    recovery.max_acceleration = motion.force;
    recovery.response_time = 1;
    recovery.slow_radius = arrival ? settings.arrival_radius : 3;
    recovery.stop_radius = 0.01f;
    recovery.lookahead = motion.horizon;
    recovery.clearance = clearance_mm;
    recovery.rectangular_clearance = true;
    bool avoidance_limited = false;
    if (motion.profile == SteeringProfile::reciprocal) {
        AvoidanceSettings avoidance;
        avoidance.max_speed = motion.speed;
        avoidance.max_acceleration = motion.force;
        avoidance.horizon = motion.horizon;
        avoidance.clearance = recovery.clearance;
        m.forces.seek = kinetra::steer(start, target, {}, recovery);
        const auto preferred = start.velocity + m.forces.seek;
        // i4sim velocities/accelerations are per tick: one control step is dt=1.
        const auto result = avoid_collisions({goals.fleet.index(m), start.position, start.velocity,
                                              bounding_radius(start), true, start.half_extents},
                                             preferred, around.agents, 1, avoidance);
        m.forces.acceleration = result.acceleration;
        m.forces.predictive = result.acceleration - m.forces.seek;
        avoidance_limited = !result.feasible;
    } else {
        m.forces.acceleration = motion.profile == SteeringProfile::experimental_rvo
                                    ? tangent_steer(m.body, target, around.neighbors, motion.speed,
                                                    motion.force, motion.horizon, arrival)
                                    : kinetra::steer(m.body, target, around.neighbors, recovery);
    }
    if (length(m.forces.collision) > 0)
        m.forces.acceleration = limited(m.forces.collision, motion.force);
    const auto acceleration = m.forces.acceleration;
    m.forces.acceleration = constrain_borders(m.body, acceleration, around.edges, settings);
    m.forces.border = m.forces.acceleration - acceleration;
    if (motion.profile == SteeringProfile::reciprocal &&
        (avoidance_limited || length(m.forces.collision) > 0 || length(m.forces.border) > 0.00001f))
        ++avoidance_limited_;
}

void MotionStep::integrate(Goals &goals, Production &production, std::size_t i, const Surroundings &around,
                           bool controlled) {
    auto &m = goals.fleet.movers[i];
    auto &tracking = goals.fleet.progress[i];
    m.body.acceleration = m.forces.acceleration;
    m.body.velocity = limited(m.body.velocity + m.body.acceleration, goals.motion.speed);
    if (goals.motion.safety_stop) {
        std::optional<std::size_t> blocker;
        const auto move = safe_move(goals, i, m.body.velocity, around, blocker);
        if (length(m.body.velocity - move) > negligible_cut_mm) {
            if (stopped_until_.size() < goals.fleet.movers.size())
                stopped_until_.resize(goals.fleet.movers.size());
            // A stop that went on in the last tick continues; one after a free tick starts.
            if (stopped_until_[i] != goals.tick) {
                ++safety_stops_;
                if (goals.log.recording()) {
                    SimulationEvent event{.tick = goals.tick, .kind = EventKind::safety_stop};
                    const auto movers = goals.fleet.movers.size();
                    event.border = blocker && *blocker >= movers;
                    event.movers = {i, blocker ? (event.border ? *blocker - movers : *blocker) : i};
                    goals.log.add(event);
                }
            }
            stopped_until_[i] = goals.tick + 1;
            ++safety_stop_ticks_;
        }
        // The mover brakes at once, harder than its force allows, as a safety stop does.
        m.body.velocity = move;
    }
    const auto before = m.body.position;
    m.body.position += m.body.velocity;
    const float moved = distance(before, m.body.position);
    m.distance += moved;
    if (m.product)
        production.add_distance(*m.product, moved);
    const float remaining = route_remaining(m.body.position, m.route);
    // Arrival slows continuously; a fixed 2 mm threshold would classify
    // the final approach as stalled even while it is still converging.
    const float progress_step = std::clamp(remaining * progress_step_fraction, 0.001f, 2.f);
    if (remaining + progress_step < tracking.best_remaining) {
        tracking.best_remaining = remaining;
        tracking.no_progress = 0;
    }
    if (m.product)
        ++m.transport_ticks;
    // A live reservation can enclose the brake target. Finish braking at the
    // current position without snapping into the invalidated route.
    if (controlled && tracking.make_way_braking && length(m.body.velocity) < stop_speed) {
        m.destination = m.body.position;
        m.body.velocity = {};
        m.body.acceleration = {};
        m.lifecycle = MoverLifecycle::idle;
        m.route.clear();
        return;
    }
    // Snap to the destination once the mover is there, slow, and the snap touches nothing.
    if (!controlled && m.route.size() == 1 &&
        distance(m.body.position, m.destination) < arrival_distance_mm &&
        length(m.body.velocity) < stop_speed &&
        goals.floor.grid().clear(m.body.position, m.destination, m.body.half_extents) && [&] {
            Body parked = m.body;
            parked.position = m.destination;
            auto touches = [&](const Body &other) {
                const auto contact = collide(parked, other);
                return contact && contact->penetration > 0;
            };
            // SAFETY STOP stops short of the gap, so the snap closes it; it only checks that
            // the place touches no neighbor where that one stands now.
            if (goals.motion.safety_stop)
                return std::none_of(around.neighbor_ids.begin(), around.neighbor_ids.end(),
                                    [&](std::size_t id) { return touches(goals.fleet.movers[id].body); });
            return std::none_of(around.neighbors.begin(), around.neighbors.end(), touches);
        }()) {
        m.body.position = m.destination;
        m.body.velocity = {};
        m.body.acceleration = {};
        m.lifecycle = MoverLifecycle::idle;
        if (m.goal == GoalKind::manual)
            m.goal = GoalKind::automatic;
        m.route.clear();
    }
}

Vec2 MotionStep::safe_move(const Goals &goals, std::size_t i, Vec2 move, const Surroundings &around,
                           std::optional<std::size_t> &blocker) const {
    const auto &movers = goals.fleet.movers;
    // Movers are unrotated boxes, so a box is its position and half extents.
    const Vec2 center = movers[i].body.position, half = movers[i].body.half_extents;
    const auto borders = goals.floor.borders();
    // An obstacle farther than the move and the gap on either axis cannot be reached in this
    // tick; most are, so the test skips them first.
    const float reach = std::max(std::abs(move.x), std::abs(move.y)) + safety_gap_mm;
    // Each obstacle cuts the move or a component of it. A cut can turn the move toward an
    // obstacle tested before, so the tests repeat until nothing changes, and a move still
    // changing after a few rounds becomes no move, which is always safe.
    auto clip = [&](Vec2 wanted, std::optional<std::size_t> &by) {
        for (int round = 0; round < 4; ++round) {
            bool changed = false;
            auto keep_clear = [&](Vec2 position, Vec2 extents, std::size_t id, float gap) {
                const Vec2 offset = center - position, extent = half + extents;
                if (std::abs(offset.x) - extent.x > reach || std::abs(offset.y) - extent.y > reach)
                    return;
                const auto next = clear_move(offset, wanted, extent, gap);
                if (next != wanted) {
                    wanted = next;
                    by = id;
                    changed = true;
                }
            };
            // Every neighbor where it stands now: those that moved this tick at their new place.
            for (const auto id : around.neighbor_ids)
                keep_clear(movers[id].body.position, movers[id].body.half_extents, id, safety_gap_mm);
            for (const auto id : around.border_ids) {
                const auto &edge = borders[id];
                keep_clear((edge.start + edge.end) / 2,
                           {std::abs(edge.end.x - edge.start.x) / 2, std::abs(edge.end.y - edge.start.y) / 2},
                           movers.size() + id, border_gap_mm);
            }
            if (!changed)
                return wanted;
        }
        return Vec2{};
    };
    const auto whole = clip(move, blocker);
    if (whole == move)
        return move;
    // Where a corner meets the move on both axes at once, which axis to cut is a guess that
    // can stop a mover that one axis alone would take past: of the cut move and the move along
    // each axis, the one that goes farthest in the wanted direction wins.
    auto best = whole;
    for (const Vec2 along : {Vec2{move.x, 0}, Vec2{0, move.y}}) {
        std::optional<std::size_t> by;
        const auto candidate = clip(along, by);
        if (dot(candidate, move) > dot(best, move)) {
            best = candidate;
            if (by)
                blocker = by;
        }
    }
    return best;
}

Aabb MotionStep::box_of(std::size_t mover, const Body &body) {
    // The bounds of a box depend on its position, extents and angle alone: a mover that stands
    // still keeps them from tick to tick. Other shapes are not remembered.
    if (body.shape != Shape::box)
        return bounds(body);
    if (boxes_of_.size() <= mover)
        boxes_of_.resize(mover + 1);
    auto &known = boxes_of_[mover];
    if (!known.valid || known.position != body.position || known.half_extents != body.half_extents ||
        known.angle != body.angle) {
        known = {true, body.position, body.half_extents, body.angle, bounds(body)};
    }
    return known.box;
}

const std::vector<std::size_t> &MotionStep::near_borders(const Goals &goals, std::size_t mover,
                                                         const Body &body) {
    if (near_.size() <= mover)
        near_.resize(mover + 1);
    auto &near = near_[mover];
    // The borders are the floor's and never change, so the same position and extents give
    // the same borders: a mover that stands still, as parked and processing ones do, skips
    // the search.
    if (near.valid && near.position == body.position && near.half_extents == body.half_extents)
        return near.borders;
    near.valid = true;
    near.position = body.position;
    near.half_extents = body.half_extents;
    near.borders.clear();
    // A border within `reach` has its bounding box within `reach` too; the margin covers the
    // rounding of the distance, so the index skips only borders the distance test rejects, and
    // returns the others in ascending order, as the loop over every border did.
    const float reach = length(body.half_extents) + .01f;
    const float limit = reach + border_search_margin_mm;
    const auto borders = goals.floor.borders();
    goals.floor.border_index().query({body.position - Vec2{limit, limit}, body.position + Vec2{limit, limit}},
                                     ids_);
    for (const std::size_t j : ids_)
        if (distance_to_segment(body.position, borders[j].start, borders[j].end) <= reach)
            near.borders.push_back(j);
    return near.borders;
}

void MotionStep::resolve_contacts(Goals &goals) {
    auto &movers = goals.fleet.movers;
    const auto borders = goals.floor.borders();
    // Count contacts before correction and resolve each unordered pair once.
    // Pairs are found in ascending order, every mover pair (i, j > i) before i's borders
    // (i, movers + j), so `current` is sorted without sorting and `contacts_` can be searched.
    auto &current = current_;
    current.clear();
    // Pairs are found on these copies, so resolving one pair never changes which pairs follow.
    // Every pair is tested on bounds computed once per mover: with about a hundred movers this
    // is cheaper than a spatial-index query per mover, and it keeps the pair order trivially.
    const std::size_t count = movers.size();
    auto &integrated = integrated_;
    auto &boxes = contact_boxes_;
    integrated.clear();
    boxes.clear();
    for (std::size_t i = 0; i < count; ++i) {
        integrated.push_back(movers[i].body);
        boxes.push_back(box_of(i, movers[i].body));
    }
    for (auto &m : movers)
        m.contacting = false;
    auto contact = [&](std::size_t a, std::size_t b) {
        movers[a].contacting = true;
        if (b < movers.size())
            movers[b].contacting = true;
        current.emplace_back(a, b);
        ++contact_ticks_;
        if (!std::binary_search(contacts_.begin(), contacts_.end(), std::pair{a, b})) {
            ++collisions_;
            if (goals.log.recording()) {
                SimulationEvent event{.tick = goals.tick, .kind = EventKind::contact};
                event.border = b >= movers.size();
                event.movers = {a, event.border ? b - movers.size() : b};
                event.passing =
                    movers[a].coordination.passing || (!event.border && movers[b].coordination.passing);
                goals.log.add(event);
            }
        }
        ++movers[a].collision_ticks;
    };
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j)
            if (boxes[i].overlaps(boxes[j]))
                if (auto c = collide(integrated[i], integrated[j]); c && c->penetration > 0) {
                    contact(i, j);
                    ++movers[j].collision_ticks;
                    auto &a = movers[i].body;
                    auto &b = movers[j].body;
                    // Held and parked movers are immovable: only the moving one is pushed out.
                    if (movers[i].held || !movers[i].moving())
                        a.motion = Motion::fixed;
                    if (movers[j].held || !movers[j].moving())
                        b.motion = Motion::fixed;
                    resolve_contact(a, b, *c);
                    a.motion = b.motion = Motion::controlled;
                }
        for (const std::size_t j : near_borders(goals, i, integrated[i])) {
            const auto &edge = borders[j];
            Body wall;
            wall.shape = Shape::box;
            wall.motion = Motion::fixed;
            wall.position = (edge.start + edge.end) / 2;
            wall.half_extents = {std::max(0.001f, std::abs(edge.end.x - edge.start.x) / 2),
                                 std::max(0.001f, std::abs(edge.end.y - edge.start.y) / 2)};
            if (auto c = collide(integrated[i], wall); c && c->penetration > 0.0011f) {
                // The thin collision proxy extends 0.001 mm into free space;
                // touching that artificial skin is not a physical wall contact.
                c->penetration -= 0.001f;
                contact(i, movers.size() + j);
                if (!movers[i].held)
                    resolve_contact(movers[i].body, wall, *c);
            }
        }
    }
    // The swap keeps both buffers' storage for the next tick.
    std::swap(contacts_, current);
}
} // namespace i4sim::detail
