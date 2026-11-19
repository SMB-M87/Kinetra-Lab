#include "geometry.h"
#include "simulation_constants.h"
#include "simulation_core.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace i4sim::detail {
using namespace kinetra;
bool SimulationCore::predictive_enabled() const {
    return motion_.predictive_passing &&
           (motion_.profile == SteeringProfile::improved || motion_.profile == SteeringProfile::reciprocal);
}
std::optional<Vec2> SimulationCore::passing_target(std::size_t mover) const {
    if (mover >= movers_.size())
        throw std::out_of_range("Mover index");
    for (const auto &plan : passing_plans_)
        for (std::size_t side = 0; side < 2; ++side)
            if (plan.movers[side] == mover)
                return plan.crossing ? plan.exit[side] : plan.entry[side];
    return std::nullopt;
}
void SimulationCore::restore_passing_routes(std::array<std::size_t, 2> ids) {
    for (auto id : ids) {
        auto &m = movers_[id];
        m.coordination.passing = false;
        m.route = plan_route(m, m.destination);
        m.lifecycle = m.route.empty() ? MoverLifecycle::unreachable : MoverLifecycle::moving;
        auto &tracking = progress(m);
        tracking.no_progress = tracking.replan = 0;
        tracking.best_remaining = 0;
        Vec2 from = m.body.position;
        for (auto point : m.route) {
            tracking.best_remaining += distance(from, point);
            from = point;
        }
        if (id < traffic_controls_.size())
            traffic_controls_[id].reset();
        if (id < passing_retry_.size())
            passing_retry_[id] = ticks_ + passing_retry_ticks;
    }
}
void SimulationCore::record_passing(EventKind kind, const PassingPlan &plan, std::optional<PassingEnd> end) {
    if (!recording_)
        return;
    SimulationEvent event{.tick = ticks_, .kind = kind, .plan = plan.id, .movers = plan.movers};
    if (kind == EventKind::passing_formed) {
        event.entry = plan.entry;
        event.exit = plan.exit;
    }
    event.end = end;
    events_.push_back(event);
}
void SimulationCore::end_passing(std::size_t index, PassingEnd end) {
    record_passing(EventKind::passing_ended, passing_plans_[index], end);
    const auto ids = passing_plans_[index].movers;
    passing_plans_.erase(passing_plans_.begin() + std::ptrdiff_t(index));
    restore_passing_routes(ids);
}
void SimulationCore::cancel_predictive_passing(Mover &m) {
    for (std::size_t i = 0; i < passing_plans_.size();) {
        const auto ids = passing_plans_[i].movers;
        if (&m != &movers_[ids[0]] && &m != &movers_[ids[1]]) {
            ++i;
            continue;
        }
        end_passing(i, PassingEnd::goal_changed);
    }
}
void SimulationCore::coordinate_predictive_passing(const SpatialGrid &index) {
    passing_retry_.resize(movers_.size());
    for (std::size_t i = 0; i < passing_plans_.size();) {
        auto &plan = passing_plans_[i];
        // The first reason found ends the plan; the event log reports it.
        std::optional<PassingEnd> end;
        if (!predictive_enabled())
            end = PassingEnd::disabled;
        bool arrived = true;
        float remaining = 0;
        for (std::size_t side = 0; side < 2; ++side) {
            const auto id = plan.movers[side];
            const auto &m = movers_[id];
            const auto target = plan.crossing ? plan.exit[side] : plan.entry[side];
            const float d = distance(m.body.position, target);
            remaining += d;
            arrived = arrived && d < arrival_distance_mm && length(m.body.velocity) < stop_speed;
            if (end)
                continue;
            if (!m.moving())
                end = PassingEnd::participant_stopped;
            else if (m.held)
                end = PassingEnd::participant_held;
            else if (traffic_controls_[id].has_value())
                end = PassingEnd::traffic_control;
        }
        if (remaining + std::max(0.0001f, motion_.speed * 0.05f) < plan.best_remaining) {
            plan.best_remaining = remaining;
            plan.progress_at = ticks_;
        }
        if (!end && ticks_ - plan.progress_at >= passing_timeout_ticks)
            end = PassingEnd::no_progress;
        if (!end && arrived && plan.crossing)
            end = PassingEnd::completed;
        if (end) {
            end_passing(i, *end);
            continue;
        }
        if (arrived) {
            plan.crossing = true;
            plan.best_remaining = std::numeric_limits<float>::infinity();
            plan.progress_at = ticks_;
            record_passing(EventKind::passing_crossing, plan);
        }
        ++i;
    }
    if (!predictive_enabled())
        return;
    std::vector<bool> claimed(movers_.size());
    for (std::size_t i = 0; i < movers_.size(); ++i)
        claimed[i] = traffic_controls_[i].has_value();
    for (const auto &plan : passing_plans_)
        for (auto id : plan.movers)
            claimed[id] = true;
    const float horizon = std::max(motion_.horizon, (4 * length(mover_max_half_) + 32) / motion_.speed +
                                                        2 * motion_.speed / motion_.force);
    const float range = 2 * motion_.speed * horizon + 2 * length(mover_max_half_);
    auto direction = [&](std::size_t id) {
        const auto &m = movers_[id];
        return m.route.empty() ? Vec2{} : normalized(m.route.front() - m.body.position);
    };
    auto eligible = [&](std::size_t id) {
        const auto &m = movers_[id];
        return !claimed[id] && m.moving() && !m.held && !m.coordination.corridor_retreating &&
               !m.route.empty() && passing_retry_[id] <= ticks_;
    };
    for (std::size_t a = 0; a < movers_.size(); ++a) {
        if (!eligible(a))
            continue;
        const auto &first = movers_[a];
        std::vector<std::pair<float, std::size_t>> candidates;
        for (auto b : index.query(
                 {first.body.position - Vec2{range, range}, first.body.position + Vec2{range, range}})) {
            if (b <= a || !eligible(b) || dot(direction(a), direction(b)) > -0.8f)
                continue;
            const Vec2 p = movers_[b].body.position - first.body.position;
            const Vec2 relative = (direction(a) - direction(b)) * motion_.speed;
            const float square = dot(relative, relative);
            if (square <= 0 || dot(p, relative) <= 0)
                continue;
            const float time = dot(p, relative) / square;
            if (time > horizon)
                continue;
            const auto closest = p - relative * time;
            const auto extent = first.body.half_extents + movers_[b].body.half_extents + Vec2{8, 8};
            if (std::abs(closest.x) < extent.x && std::abs(closest.y) < extent.y)
                candidates.push_back({time, b});
        }
        std::sort(candidates.begin(), candidates.end());
        for (const auto &[time, b] : candidates) {
            (void)time;
            const auto &second = movers_[b];
            const auto axis = normalized(direction(a) - direction(b));
            const auto separation = dot(second.body.position - first.body.position, axis);
            const float along = support(first.body, axis) + support(second.body, axis);
            // The formation segments remain longitudinally separated, even when
            // the two movers take different amounts of time to reach their lanes.
            if (separation < 2 * along + 32)
                continue;
            const auto midpoint = (first.body.position + second.body.position) / 2;
            bool accepted = false;
            for (float hand : {-1.f, 1.f}) {
                const auto lateral = perpendicular(axis) * hand;
                const float step = std::max(support(first.body, lateral), support(second.body, lateral)) + 8;
                for (float shift : {0.f, step, -step}) {
                    PassingPlan plan;
                    plan.movers = {a, b};
                    plan.progress_at = ticks_;
                    plan.best_remaining = std::numeric_limits<float>::infinity();
                    for (std::size_t side = 0; side < 2; ++side) {
                        const auto &m = movers_[plan.movers[side]];
                        const float sign = side == 0 ? 1.f : -1.f;
                        const float lane = sign * (support(m.body, lateral) + 8);
                        plan.entry[side] =
                            m.body.position + axis * (sign * along / 2) +
                            lateral * (dot(midpoint - m.body.position, lateral) + lane + shift);
                        plan.exit[side] = midpoint + axis * (sign * (along + 16)) + lateral * (lane + shift);
                    }
                    if (dot(first.destination - plan.exit[0], axis) < 8 ||
                        dot(second.destination - plan.exit[1], -axis) < 8)
                        continue;
                    bool clear = true;
                    for (std::size_t side = 0; side < 2 && clear; ++side) {
                        const auto id = plan.movers[side];
                        auto moving = movers_[id].body;
                        moving.half_extents += clearance;
                        Vec2 from = moving.position;
                        for (auto to : {plan.entry[side], plan.exit[side]}) {
                            if (!floor_.grid().clear(from, to, moving.half_extents)) {
                                clear = false;
                                break;
                            }
                            moving.position = from;
                            const auto area = travel_bounds(from, to, moving.half_extents);
                            for (auto other : index.query(area))
                                if (other != a && other != b &&
                                    sweep_translation(moving, to - from, movers_[other].body))
                                    clear = false;
                            for (const auto &zone : traffic_zones_) {
                                Body obstacle;
                                obstacle.shape = Shape::box;
                                obstacle.motion = Motion::fixed;
                                obstacle.position = zone.center;
                                obstacle.half_extents = zone.half_extents + clearance;
                                if (sweep_translation(moving, to - from, obstacle))
                                    clear = false;
                            }
                            for (const auto &other : passing_plans_)
                                for (const auto &reserved : other.space)
                                    if (reserved.overlaps(area))
                                        clear = false;
                            plan.space.push_back(area);
                            from = to;
                        }
                    }
                    if (!clear)
                        continue;
                    plan.id = next_plan_++;
                    record_passing(EventKind::passing_formed, plan);
                    passing_plans_.push_back(std::move(plan));
                    claimed[a] = claimed[b] = true;
                    accepted = true;
                    break;
                }
                if (accepted)
                    break;
            }
            if (accepted)
                break;
        }
    }
    for (const auto &plan : passing_plans_)
        for (std::size_t side = 0; side < 2; ++side) {
            const auto id = plan.movers[side];
            movers_[id].coordination.passing = true;
            traffic_controls_[id] = TrafficControl{std::numeric_limits<std::size_t>::max(),
                                                   plan.crossing ? plan.exit[side] : plan.entry[side], false};
        }
}
} // namespace i4sim::detail
