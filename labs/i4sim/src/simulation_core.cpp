#include "simulation_core.h"
#include "floor_plan.h"
#include "geometry.h"
#include "goals.h"
#include "production.h"
#include "routing.h"
#include "simulation_constants.h"
#include "traffic.h"
#include <fstream>
#include <kinetra/avoidance.h>
#include <kinetra/navigation.h>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace i4sim::detail {
using namespace kinetra;
bool SimulationCore::stalled(std::size_t index) const {
    const auto &m = fleet_.movers.at(index);
    return fleet_.progress[index].no_progress >= stall_ticks && !m.coordination.corridor_retreating &&
           !m.coordination.corridor_waiting && !m.coordination.passing;
}
SimulationCore::SimulationCore(Blueprint b, std::uint32_t seed)
    : blueprint_(std::move(b)), floor_(blueprint_), traffic_(floor_), production_(blueprint_, seed),
      seed_(seed) {
    for (const auto &definition : blueprint_.movers) {
        Mover m;
        m.definition = definition;
        m.body.shape = Shape::box;
        m.body.motion = Motion::controlled;
        m.body.half_extents = mover_spec(definition.model).dimension / 2;
        fleet_.max_half.x = std::max(fleet_.max_half.x, m.body.half_extents.x);
        fleet_.max_half.y = std::max(fleet_.max_half.y, m.body.half_extents.y);
        m.body.position = definition.position + m.body.half_extents;
        m.parking = m.destination = m.previous_position = m.body.position;
        if (floor_.grid().blocked(floor_.grid().cell(m.parking)))
            throw std::runtime_error("Mover starts in a forbidden cell: " + definition.id);
        fleet_.movers.push_back(m);
    }
    fleet_.progress.resize(fleet_.movers.size());
    floor_.update_heatmap(fleet_.movers, motion_.heatmap);
}
void SimulationCore::configure_motion(MotionSettings settings) {
    if (int(settings.profile) < int(SteeringProfile::improved) ||
        int(settings.profile) > int(SteeringProfile::reciprocal) || !std::isfinite(settings.speed) ||
        settings.speed <= 0 || !std::isfinite(settings.force) || settings.force <= 0 ||
        !std::isfinite(settings.horizon) || settings.horizon <= 0 || settings.horizon > 64)
        throw std::invalid_argument("Invalid motion settings");
    traffic_.configure(floor_, settings);
    for (auto &m : fleet_.movers) {
        auto &coordination = m.coordination;
        if (!settings.junction_traffic)
            coordination.junction_waiting = false;
        if (!settings.corridor_traffic)
            coordination.corridor_waiting = coordination.corridor_retreating = false;
        if (!coordination.junction_waiting && !coordination.corridor_waiting)
            coordination.exit_waiting = false;
    }
    fleet_.stops.clear();
    motion_ = settings;
    floor_.update_heatmap(fleet_.movers, motion_.heatmap);
    for (auto &m : fleet_.movers) {
        m.body.velocity = limited(m.body.velocity, settings.speed);
        if (!settings.parking_traffic && m.goal == GoalKind::parking_staging)
            goals().set(m, m.parking);
        else if (m.moving()) {
            // Setting the same goal again replans the route but keeps its origin.
            const auto goal = m.goal;
            goals().set(m, m.destination);
            m.goal = goal;
        }
    }
}
void SimulationCore::single_step() {
    if (state_ != RunState::paused)
        return;
    state_ = RunState::running;
    tick();
    if (state_ == RunState::running)
        state_ = RunState::paused;
}
void SimulationCore::run() {
    if (state_ == RunState::paused)
        state_ = RunState::running;
}
void SimulationCore::pause() {
    if (state_ == RunState::running)
        state_ = RunState::paused;
}
void SimulationCore::updates_per_second(unsigned rate) {
    if (rate < 1 || rate > 10000)
        throw std::invalid_argument("UPS must be in 1..10000");
    ups_ = rate;
    clock_ = FixedClock(1.0 / rate, 256);
    // The new clock starts with no accumulated time; blending from the last tick's start
    // would draw the movers a tick back.
    settle_previous_positions();
}
void SimulationCore::settle_previous_positions() {
    for (auto &m : fleet_.movers)
        m.previous_position = m.body.position;
}
void SimulationCore::advance(double elapsed) {
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::invalid_argument("Invalid elapsed time");
    if (!running()) {
        resume_pending_ = true;
        return;
    }
    if (resume_pending_) {
        settle_previous_positions();
        resume_pending_ = false;
    }
    const int count = clock_.advance(elapsed);
    for (int i = 0; i < count && running(); ++i)
        tick();
}
std::uint64_t SimulationCore::add_product(ProductKind kind) {
    return production_.add_product(kind, fleet_.movers.size(), ticks_);
}
void SimulationCore::toggle_mover(std::size_t index) {
    auto &m = fleet_.movers.at(index);
    m.held = !m.held;
    if (m.held)
        m.body.velocity = {};
}
void SimulationCore::toggle_producer(std::size_t index) {
    production_.toggle_producer(index);
}
bool SimulationCore::move_to(std::size_t index, Vec2 target) {
    auto &m = fleet_.movers.at(index);
    if (m.held || m.product || floor_.grid().blocked(floor_.grid().cell(target)))
        return false;
    auto route = floor_.grid().path(m.body.position, target, m.body.half_extents);
    if (route.empty())
        return false;
    goals().set(m, target);
    m.goal = GoalKind::manual;
    return true;
}
std::size_t SimulationCore::parking_waiting() const {
    return std::count_if(fleet_.movers.begin(), fleet_.movers.end(),
                         [](const Mover &m) { return m.goal == GoalKind::parking_staging; });
}
bool SimulationCore::prepare_route(Mover &m) {
    if (m.held || !m.moving())
        return false;
    // Congestion includes this mover's footprint; it is not evidence that
    // its route is obstructed. Replan only after sustained lack of progress.
    if (!m.detour && ++progress(m).replan >= replan_ticks && progress(m).no_progress >= obstruction_ticks) {
        auto route = plan_route(floor_, fleet_, fleet_.index(m), m.destination);
        if (!route.empty()) {
            m.route = std::move(route);
            progress(m).best_remaining = route_remaining(m.body.position, m.route);
        }
        progress(m).replan = 0;
    }
    if (m.route.empty()) {
        goals().set(m, m.destination);
        if (m.lifecycle == MoverLifecycle::unreachable)
            return false;
    }
    if (++progress(m).no_progress >=
            stall_ticks +
                std::min(recovery_backoff_limit, progress(m).recovery_attempts) * recovery_backoff_ticks &&
        motion_.recovery) {
        recover_route(floor_, fleet_, fleet_.index(m));
        ++progress(m).recovery_attempts;
    }
    while (m.route.size() > 1 && distance(m.body.position, m.route.front()) < waypoint_reach_mm &&
           floor_.grid().clear(m.body.position, m.route[1], m.body.half_extents))
        m.route.erase(m.route.begin());
    return true;
}
Vec2 SimulationCore::junction_center(std::size_t index) const {
    if (index >= floor_.junction_count())
        throw std::out_of_range("Junction index");
    return floor_.zones()[index].center;
}
std::size_t SimulationCore::corridor_waiting() const {
    return std::count_if(fleet_.movers.begin(), fleet_.movers.end(),
                         [](const Mover &m) { return m.coordination.corridor_waiting; });
}
Aabb SimulationCore::corridor_bounds(std::size_t index) const {
    if (index >= corridor_count())
        throw std::out_of_range("Corridor index");
    const auto &zone = floor_.zones()[floor_.junction_count() + index];
    return {zone.center - zone.half_extents, zone.center + zone.half_extents};
}
std::optional<Aabb> SimulationCore::corridor_exit_reservation(std::size_t index) const {
    if (index >= corridor_count())
        throw std::out_of_range("Corridor index");
    return traffic_.exit_reservation(floor_.junction_count() + index);
}
std::size_t SimulationCore::junction_waiting() const {
    return std::count_if(fleet_.movers.begin(), fleet_.movers.end(),
                         [](const Mover &m) { return m.coordination.junction_waiting; });
}
std::size_t SimulationCore::junction_exit_waiting() const {
    return std::count_if(fleet_.movers.begin(), fleet_.movers.end(),
                         [](const Mover &m) { return m.coordination.exit_waiting; });
}
std::optional<Aabb> SimulationCore::junction_exit_reservation(std::size_t index) const {
    if (index >= floor_.junction_count())
        throw std::out_of_range("Junction index");
    return traffic_.exit_reservation(index);
}
std::optional<Vec2> SimulationCore::passing_target(std::size_t mover) const {
    if (mover >= fleet_.movers.size())
        throw std::out_of_range("Mover index");
    return passing_.target(mover);
}
std::size_t SimulationCore::corridor_recovering() const {
    return traffic_.recovering();
}
std::optional<Vec2> SimulationCore::corridor_retreat_target(std::size_t mover) const {
    if (mover >= fleet_.movers.size())
        throw std::out_of_range("Mover index");
    return traffic_.retreat_target(mover);
}
void SimulationCore::coordinate_traffic(const SpatialGrid &index, std::span<const Aabb> bodies) {
    // Corridor recovery and passing claim movers first; admission then skips their zones.
    Traffic::begin_tick(fleet_);
    if (motion_.corridor_traffic)
        traffic_.recover_corridors(floor_, fleet_, ticks_, index);
    passing_.coordinate(floor_, fleet_, motion_, ticks_, index, log_);
    traffic_.admit(floor_, fleet_, motion_, index, bodies);
}
void SimulationCore::move_agents() {
    const bool prepare_together = (motion_.junction_traffic && floor_.junction_count() > 0) ||
                                  (motion_.corridor_traffic && corridor_count() > 0) ||
                                  (PredictivePassing::enabled(motion_) && fleet_.movers.size() > 1);
    if (prepare_together)
        for (auto &m : fleet_.movers)
            prepare_route(m);
    avoidance_limited_ = 0;
    std::vector<Body> snapshot;
    std::vector<Aabb> boxes;
    for (const auto &m : fleet_.movers) {
        snapshot.push_back(m.body);
        if (m.held || !m.moving()) {
            snapshot.back().motion = Motion::fixed;
            snapshot.back().velocity = {};
        }
        boxes.push_back(bounds(m.body));
    }
    SpatialGrid index(std::max(blueprint_.cell_size.x, blueprint_.cell_size.y));
    index.rebuild(boxes);
    coordinate_traffic(index, boxes);
    for (std::size_t i = 0; i < fleet_.movers.size(); ++i) {
        auto &m = fleet_.movers[i];
        if (!m.product)
            ++m.idle_ticks;
        if (m.held || !m.moving())
            continue;
        if (!prepare_together && !prepare_route(m))
            continue;
        std::vector<Body> neighbors;
        std::vector<AvoidanceAgent> avoidance_neighbors;
        const float reach = length(m.body.half_extents) + blueprint_.mover_max_extent * 2 +
                            2 * motion_.speed * motion_.horizon;
        for (auto id :
             index.query({m.body.position - Vec2{reach, reach}, m.body.position + Vec2{reach, reach}}))
            if (id != i) {
                neighbors.push_back(snapshot[id]);
                if (motion_.profile == SteeringProfile::reciprocal) {
                    const auto &other = snapshot[id];
                    avoidance_neighbors.push_back({id, other.position, other.velocity, bounding_radius(other),
                                                   other.motion != Motion::fixed &&
                                                       !fleet_.movers[id].coordination.junction_waiting &&
                                                       !fleet_.movers[id].coordination.corridor_waiting,
                                                   other.half_extents});
                }
            }
        std::vector<Segment> edges;
        for (const auto &edge : floor_.borders())
            if (distance_to_segment(m.body.position, edge.start, edge.end) <=
                length(m.body.half_extents) + motion_.speed * motion_.horizon + 10)
                edges.push_back(edge);
        if (i < fleet_.stops.size())
            edges.insert(edges.end(), fleet_.stops[i].begin(), fleet_.stops[i].end());
        const auto &control = fleet_.controls[i];
        const auto target = control ? control->target : m.route.front();
        const bool arrival = control ? control->arrival : m.route.size() == 1;
        PrioritySteering settings;
        settings.max_speed = motion_.speed;
        settings.max_acceleration = motion_.force;
        settings.prediction_steps = int(std::ceil(motion_.horizon));
        settings.arrival_radius = length(m.body.half_extents * 2) * 0.45f;
        m.forces = priority_steer(m.body, target, neighbors, edges, settings, arrival);
        if (motion_.profile != SteeringProfile::original) {
            Steering recovery;
            recovery.max_speed = motion_.speed;
            recovery.max_acceleration = motion_.force;
            recovery.response_time = 1;
            recovery.slow_radius = arrival ? settings.arrival_radius : 3;
            recovery.stop_radius = 0.01f;
            recovery.lookahead = motion_.horizon;
            recovery.clearance = clearance_mm;
            recovery.rectangular_clearance = true;
            bool avoidance_limited = false;
            if (motion_.profile == SteeringProfile::reciprocal) {
                AvoidanceSettings avoidance;
                avoidance.max_speed = motion_.speed;
                avoidance.max_acceleration = motion_.force;
                avoidance.horizon = motion_.horizon;
                avoidance.clearance = recovery.clearance;
                const auto &body = snapshot[i];
                m.forces.seek = steer(body, target, {}, recovery);
                const auto preferred = body.velocity + m.forces.seek;
                // i4sim velocities/accelerations are per tick: one control step is dt=1.
                const auto result = avoid_collisions(
                    {i, body.position, body.velocity, bounding_radius(body), true, body.half_extents},
                    preferred, avoidance_neighbors, 1, avoidance);
                m.forces.acceleration = result.acceleration;
                m.forces.predictive = result.acceleration - m.forces.seek;
                avoidance_limited = !result.feasible;
            } else {
                m.forces.acceleration = motion_.profile == SteeringProfile::experimental_rvo
                                            ? tangent_steer(m.body, target, neighbors, motion_.speed,
                                                            motion_.force, motion_.horizon, arrival)
                                            : steer(m.body, target, neighbors, recovery);
            }
            if (length(m.forces.collision) > 0)
                m.forces.acceleration = limited(m.forces.collision, motion_.force);
            const auto acceleration = m.forces.acceleration;
            m.forces.acceleration = constrain_borders(m.body, acceleration, edges, settings);
            m.forces.border = m.forces.acceleration - acceleration;
            if (motion_.profile == SteeringProfile::reciprocal &&
                (avoidance_limited || length(m.forces.collision) > 0 || length(m.forces.border) > 0.00001f))
                ++avoidance_limited_;
        }
        m.body.acceleration = m.forces.acceleration;
        m.body.velocity = limited(m.body.velocity + m.body.acceleration, settings.max_speed);
        const auto before = m.body.position;
        m.body.position += m.body.velocity;
        const float moved = distance(before, m.body.position);
        m.distance += moved;
        if (m.product)
            production_.add_distance(*m.product, moved);
        const float remaining = route_remaining(m.body.position, m.route);
        // Arrival slows continuously; a fixed 2 mm threshold would classify
        // the final approach as stalled even while it is still converging.
        const float progress_step = std::clamp(remaining * progress_step_fraction, 0.001f, 2.f);
        if (remaining + progress_step < progress(m).best_remaining) {
            progress(m).best_remaining = remaining;
            progress(m).no_progress = 0;
        }
        if (m.product)
            ++m.transport_ticks;
        if (!control && m.route.size() == 1 &&
            distance(m.body.position, m.destination) < arrival_distance_mm &&
            length(m.body.velocity) < stop_speed &&
            floor_.grid().clear(m.body.position, m.destination, m.body.half_extents) &&
            std::none_of(neighbors.begin(), neighbors.end(), [&](const Body &other) {
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
    // Count contacts before correction and resolve each unordered pair once.
    std::set<std::pair<std::size_t, std::size_t>> current;
    std::vector<Body> integrated;
    for (const auto &m : fleet_.movers)
        integrated.push_back(m.body);
    for (auto &m : fleet_.movers)
        m.contacting = false;
    auto contact = [&](std::size_t a, std::size_t b) {
        fleet_.movers[a].contacting = true;
        if (b < fleet_.movers.size())
            fleet_.movers[b].contacting = true;
        current.emplace(a, b);
        ++contact_ticks_;
        if (!contacts_.contains({a, b})) {
            ++collisions_;
            if (log_.recording()) {
                SimulationEvent event{.tick = ticks_, .kind = EventKind::contact};
                event.border = b >= fleet_.movers.size();
                event.movers = {a, event.border ? b - fleet_.movers.size() : b};
                event.passing = fleet_.movers[a].coordination.passing ||
                                (!event.border && fleet_.movers[b].coordination.passing);
                log_.add(event);
            }
        }
        ++fleet_.movers[a].collision_ticks;
    };
    for (std::size_t i = 0; i < fleet_.movers.size(); ++i) {
        for (std::size_t j = i + 1; j < fleet_.movers.size(); ++j)
            if (bounds(integrated[i]).overlaps(bounds(integrated[j])))
                if (auto c = collide(integrated[i], integrated[j]); c && c->penetration > 0) {
                    contact(i, j);
                    ++fleet_.movers[j].collision_ticks;
                    auto &a = fleet_.movers[i].body;
                    auto &b = fleet_.movers[j].body;
                    if (fleet_.movers[i].held || !fleet_.movers[i].moving())
                        a.motion = Motion::fixed;
                    if (fleet_.movers[j].held || !fleet_.movers[j].moving())
                        b.motion = Motion::fixed;
                    resolve_contact(a, b, *c);
                    a.motion = b.motion = Motion::controlled;
                }
        for (std::size_t j = 0; j < floor_.borders().size(); ++j) {
            const auto &edge = floor_.borders()[j];
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
                contact(i, fleet_.movers.size() + j);
                if (!fleet_.movers[i].held)
                    resolve_contact(fleet_.movers[i].body, wall, *c);
            }
        }
    }
    contacts_ = std::move(current);
}
void SimulationCore::tick() {
    if (!running())
        return;
    if (ticks_ >= blueprint_.tick_cap) {
        state_ = RunState::finished;
        return;
    }
    ++ticks_;
    for (auto &m : fleet_.movers)
        m.previous_position = m.body.position;
    if (workload_)
        production_.generate(fleet_.movers.size(), ticks_);
    if (ticks_ % heatmap_period_ticks == 1)
        floor_.update_heatmap(fleet_.movers, motion_.heatmap);
    goals().coordinate_parking();
    move_agents();
    auto goals_now = goals();
    production_.update(goals_now);
    if (ticks_ >= blueprint_.tick_cap)
        state_ = RunState::finished;
}
void SimulationCore::dump(const std::filesystem::path &directory) const {
    using Json = nlohmann::json;
    std::filesystem::create_directories(directory);
    Json data{
        {"blueprint", blueprint_.name},   {"ticks", ticks_},           {"completed", production_.completed()},
        {"contactTicks", contact_ticks_}, {"collisions", collisions_}, {"movers", Json::array()},
        {"producers", Json::array()},     {"products", Json::array()}};
    for (const auto &m : fleet_.movers)
        data["movers"].push_back({{"id", m.definition.id},
                                  {"distance", m.distance},
                                  {"idleTicks", m.idle_ticks},
                                  {"transportTicks", m.transport_ticks},
                                  {"interactions", m.interactions},
                                  {"blocked", m.held}});
    for (const auto &p : production_.producers())
        data["producers"].push_back({{"id", p.definition.id},
                                     {"interactions", p.interactions},
                                     {"processingTicks", p.processing_ticks},
                                     {"idleTicks", p.idle_ticks},
                                     {"queue", p.queue},
                                     {"blocked", p.blocked}});
    auto record = [&](const Product &p) {
        data["products"].push_back(
            {{"id", p.id},
             {"kind", p.kind == ProductKind::trimmer ? "Trimmer" : "TrimmerPersonalized"},
             {"step", p.step},
             {"state", int(p.state)},
             {"created", p.created},
             {"finished", p.finished},
             {"transportTicks", p.transport_ticks},
             {"processingTicks", p.processing_ticks},
             {"distance", p.distance}});
    };
    for (const auto &p : production_.completed_products())
        record(p);
    for (const auto &p : production_.products())
        record(p);
    std::ofstream out(directory / "summary.json");
    out << data.dump(2) << '\n';
    if (!out)
        throw std::runtime_error("Cannot write simulation summary: " + directory.string());
}
} // namespace i4sim::detail
