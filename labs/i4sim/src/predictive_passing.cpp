#include "simulation.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace i4sim {
using namespace kinetra;
namespace {
float support(const Body &body, Vec2 direction) {
    return std::abs(direction.x) * body.half_extents.x + std::abs(direction.y) * body.half_extents.y;
}
Aabb travel_bounds(Vec2 from, Vec2 to, Vec2 extent) {
    return {{std::min(from.x, to.x) - extent.x, std::min(from.y, to.y) - extent.y},
            {std::max(from.x, to.x) + extent.x, std::max(from.y, to.y) + extent.y}};
}
} // namespace
bool Simulation::predictive_enabled() const {
    return motion_.predictive_passing &&
           (motion_.profile == SteeringProfile::improved || motion_.profile == SteeringProfile::reciprocal);
}
std::optional<Vec2> Simulation::passing_target(std::size_t mover) const {
    if (mover >= movers_.size())
        throw std::out_of_range("Mover index");
    for (const auto &plan : passing_plans_)
        for (std::size_t side = 0; side < 2; ++side)
            if (plan.movers[side] == mover)
                return plan.crossing ? plan.exit[side] : plan.entry[side];
    return std::nullopt;
}
void Simulation::restore_passing_routes(std::array<std::size_t, 2> ids) {
    for (auto id : ids) {
        auto &m = movers_[id];
        m.passing_detour = false;
        m.route = plan_route(m, m.destination);
        m.unreachable = m.route.empty();
        m.moving = !m.unreachable;
        m.no_progress = m.replan = 0;
        m.best_remaining = 0;
        Vec2 from = m.body.position;
        for (auto point : m.route) {
            m.best_remaining += distance(from, point);
            from = point;
        }
        if (id < traffic_controls_.size())
            traffic_controls_[id].reset();
        if (id < passing_retry_.size())
            passing_retry_[id] = ticks_ + 120;
    }
}
void Simulation::cancel_predictive_passing(Mover &m) {
    for (std::size_t i = 0; i < passing_plans_.size();) {
        const auto ids = passing_plans_[i].movers;
        if (&m != &movers_[ids[0]] && &m != &movers_[ids[1]]) {
            ++i;
            continue;
        }
        passing_plans_.erase(passing_plans_.begin() + i);
        restore_passing_routes(ids);
    }
}
void Simulation::coordinate_predictive_passing(const SpatialGrid &index) {
    passing_retry_.resize(movers_.size());
    for (auto &m : movers_)
        m.passing_detour = false;
    for (std::size_t i = 0; i < passing_plans_.size();) {
        auto &plan = passing_plans_[i];
        bool cancel = !predictive_enabled();
        bool arrived = true;
        float remaining = 0;
        for (std::size_t side = 0; side < 2; ++side) {
            const auto id = plan.movers[side];
            const auto &m = movers_[id];
            const auto target = plan.crossing ? plan.exit[side] : plan.entry[side];
            const float d = distance(m.body.position, target);
            remaining += d;
            arrived = arrived && d < 0.1f && length(m.body.velocity) < 0.05f;
            cancel = cancel || !m.moving || m.blocked || traffic_controls_[id].has_value();
        }
        if (remaining + std::max(0.0001f, motion_.speed * 0.05f) < plan.best_remaining) {
            plan.best_remaining = remaining;
            plan.progress_at = ticks_;
        }
        cancel = cancel || ticks_ - plan.progress_at >= 240;
        if (cancel || (arrived && plan.crossing)) {
            const auto ids = plan.movers;
            passing_plans_.erase(passing_plans_.begin() + i);
            restore_passing_routes(ids);
            continue;
        }
        if (arrived) {
            plan.crossing = true;
            plan.best_remaining = std::numeric_limits<float>::infinity();
            plan.progress_at = ticks_;
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
        return !claimed[id] && m.moving && !m.blocked && !m.corridor_retreating && !m.route.empty() &&
               passing_retry_[id] <= ticks_;
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
                        moving.half_extents += Vec2{2, 2};
                        Vec2 from = moving.position;
                        for (auto to : {plan.entry[side], plan.exit[side]}) {
                            if (!grid_.clear(from, to, moving.half_extents)) {
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
                                obstacle.half_extents = zone.half_extents + Vec2{2, 2};
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
            movers_[id].passing_detour = true;
            traffic_controls_[id] = TrafficControl{std::numeric_limits<std::size_t>::max(),
                                                   plan.crossing ? plan.exit[side] : plan.entry[side], false};
        }
}
} // namespace i4sim
