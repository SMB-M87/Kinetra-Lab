#include "i4sim/simulation/detail/motion_step.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/routing.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <kinetra/simulation/steering.h>

#include <algorithm>
#include <cmath>

namespace i4sim::detail {
using namespace kinetra;
namespace {
/// Margin in millimeters beyond the steering reach within which borders are considered.
constexpr float border_search_margin_mm = 10;
} // namespace

void MotionStep::run(Goals &goals, Production &production) {
    auto &fleet = goals.fleet;
    const auto &motion = goals.motion;
    // With coordination on, every route is prepared before coordination reads them.
    const bool prepare_together = (motion.junction_traffic && goals.floor.junction_count() > 0) ||
                                  (motion.corridor_traffic && goals.floor.corridor_count() > 0) ||
                                  (PredictivePassing::enabled(motion) && fleet.movers.size() > 1);
    if (prepare_together)
        for (std::size_t i = 0; i < fleet.movers.size(); ++i)
            follow_route(goals, i);
    avoidance_limited_ = 0;
    std::vector<Body> snapshot;
    std::vector<Aabb> boxes;
    for (const auto &m : fleet.movers) {
        snapshot.push_back(m.body);
        if (m.held || !m.moving()) {
            snapshot.back().motion = Motion::fixed;
            snapshot.back().velocity = {};
        }
        boxes.push_back(bounds(m.body));
    }
    SpatialGrid index(std::max(goals.floor.cell_size().x, goals.floor.cell_size().y));
    index.rebuild(boxes);
    // Corridor recovery and passing claim movers first; admission then skips their zones.
    Traffic::begin_tick(fleet);
    if (motion.corridor_traffic)
        goals.traffic.recover_corridors(goals.floor, fleet, goals.tick, index);
    goals.passing.coordinate(goals.floor, fleet, motion, goals.tick, index, goals.log);
    goals.traffic.admit(goals.floor, fleet, motion, index, boxes);
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        auto &m = fleet.movers[i];
        if (!m.product)
            ++m.idle_ticks;
        if (m.held || !m.moving())
            continue;
        if (!prepare_together && !follow_route(goals, i))
            continue;
        const auto around = surroundings(goals, i, snapshot, index);
        const auto &control = fleet.controls[i];
        const auto target = control ? control->target : m.route.front();
        const bool arrival = control ? control->arrival : m.route.size() == 1;
        choose_acceleration(goals, m, snapshot[i], around, target, arrival);
        integrate(goals, production, i, around, control.has_value());
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
    if (!m.detour && ++tracking.replan >= replan_ticks && tracking.no_progress >= obstruction_ticks) {
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
        goals.motion.recovery) {
        recover_route(goals.floor, goals.fleet, i);
        ++tracking.recovery_attempts;
    }
    while (m.route.size() > 1 && distance(m.body.position, m.route.front()) < waypoint_reach_mm &&
           goals.floor.grid().clear(m.body.position, m.route[1], m.body.half_extents))
        m.route.erase(m.route.begin());
    return true;
}

MotionStep::Surroundings MotionStep::surroundings(const Goals &goals, std::size_t i,
                                                  const std::vector<Body> &snapshot,
                                                  const SpatialGrid &index) {
    const auto &fleet = goals.fleet;
    const auto &motion = goals.motion;
    const auto &m = fleet.movers[i];
    Surroundings around;
    const float reach =
        length(m.body.half_extents) + goals.floor.mover_max_extent() * 2 + 2 * motion.speed * motion.horizon;
    for (auto id : index.query({m.body.position - Vec2{reach, reach}, m.body.position + Vec2{reach, reach}}))
        if (id != i) {
            around.neighbors.push_back(snapshot[id]);
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
    for (const auto &edge : goals.floor.borders())
        if (distance_to_segment(m.body.position, edge.start, edge.end) <=
            length(m.body.half_extents) + motion.speed * motion.horizon + border_search_margin_mm)
            around.edges.push_back(edge);
    if (i < fleet.stops.size())
        around.edges.insert(around.edges.end(), fleet.stops[i].begin(), fleet.stops[i].end());
    return around;
}

void MotionStep::choose_acceleration(const Goals &goals, Mover &m, const Body &start,
                                     const Surroundings &around, Vec2 target, bool arrival) {
    const auto &motion = goals.motion;
    PrioritySteering settings;
    settings.max_speed = motion.speed;
    settings.max_acceleration = motion.force;
    settings.prediction_steps = int(std::ceil(motion.horizon));
    settings.arrival_radius = length(m.body.half_extents * 2) * 0.45f;
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
    // Snap to the destination once the mover is there, slow, and the snap touches nothing.
    if (!controlled && m.route.size() == 1 &&
        distance(m.body.position, m.destination) < arrival_distance_mm &&
        length(m.body.velocity) < stop_speed &&
        goals.floor.grid().clear(m.body.position, m.destination, m.body.half_extents) &&
        std::none_of(around.neighbors.begin(), around.neighbors.end(), [&](const Body &other) {
            Body parked = m.body;
            parked.position = m.destination;
            const auto contact = collide(parked, other);
            return contact && contact->penetration > 0;
        })) {
        m.body.position = m.destination;
        m.body.velocity = {};
        m.body.acceleration = {};
        m.lifecycle = MoverLifecycle::idle;
        if (m.goal == GoalKind::manual)
            m.goal = GoalKind::automatic;
        m.route.clear();
    }
}

void MotionStep::resolve_contacts(Goals &goals) {
    auto &movers = goals.fleet.movers;
    const auto borders = goals.floor.borders();
    // Count contacts before correction and resolve each unordered pair once.
    std::set<std::pair<std::size_t, std::size_t>> current;
    std::vector<Body> integrated;
    for (const auto &m : movers)
        integrated.push_back(m.body);
    for (auto &m : movers)
        m.contacting = false;
    auto contact = [&](std::size_t a, std::size_t b) {
        movers[a].contacting = true;
        if (b < movers.size())
            movers[b].contacting = true;
        current.emplace(a, b);
        ++contact_ticks_;
        if (!contacts_.contains({a, b})) {
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
    for (std::size_t i = 0; i < movers.size(); ++i) {
        for (std::size_t j = i + 1; j < movers.size(); ++j)
            if (bounds(integrated[i]).overlaps(bounds(integrated[j])))
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
        for (std::size_t j = 0; j < borders.size(); ++j) {
            const auto &edge = borders[j];
            if (distance_to_segment(integrated[i].position, edge.start, edge.end) >
                length(integrated[i].half_extents) + .01f)
                continue;
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
    contacts_ = std::move(current);
}
} // namespace i4sim::detail
