#include "demo/world/agent.h"

#include <algorithm>
#include <cmath>

namespace demo {
Agent::Agent(std::size_t body_id, kinetra::Vec2 home, std::uint32_t seed)
    : body_id_(body_id), home_(home), goal_(home), random_(seed ^ (std::uint32_t(body_id) * 2654435761u)) {
    heading_ = unit() * full_turn;
}

void Agent::retarget(kinetra::Vec2 destination) {
    if (!active_)
        return;
    goal_ = destination;
    wandering_ = false;
    command_remaining_ = 20;
    invalidate_route();
}

void Agent::update_visibility(kinetra::Aabb view, const kinetra::Body &body) {
    const bool was_active = active_;
    active_ = view.overlaps(kinetra::bounds(body));
    if (!active_) {
        route_.clear();
        steers_without_route_ = false;
    } else if (!was_active)
        invalidate_route();
}

kinetra::Vec2 Agent::advance_navigation(float dt, const kinetra::Body &body,
                                        const kinetra::Navigation &navigation, const Tuning &tuning) {
    if (!active_)
        return body.position;
    if (!wandering_) {
        command_remaining_ -= dt;
        if (command_remaining_ <= 0) {
            wandering_ = true;
            wander_remaining_ = 0;
        }
    }
    replan_in_ -= dt;
    wander_remaining_ -= dt;
    if ((!wandering_ || !route_.empty()) && kinetra::length(goal_ - body.position) < 0.6f) {
        wandering_ = true;
        wander_remaining_ = 0;
    }
    steers_without_route_ = wandering_ && tuning.wander.mode == WanderMode::steering;
    if (steers_without_route_)
        return steer_wander(dt, body, tuning);
    if (wandering_ && (wander_remaining_ <= 0 || (route_.empty() && replan_in_ <= 0))) {
        choose_wander_goal(body, navigation, tuning.wander);
    } else if (replan_in_ <= 0) {
        route_ = navigation.path(body.position, goal_);
        if (!route_.empty())
            goal_ = route_.back();
        replan_in_ = replan_interval;
    }
    return route_target(body.position, route_, navigation);
}

kinetra::Vec2 Agent::steer_wander(float dt, const kinetra::Body &body, const Tuning &tuning) {
    using namespace kinetra;
    const WanderTuning &wander = tuning.wander;
    route_.clear();
    // Idle timers: a switch to route wander plans its first leg at once.
    replan_in_ = wander_remaining_ = 0;
    wander_angle_ = std::remainder(wander_angle_ + (unit() * 2 - 1) * wander.jitter * dt, full_turn);
    if (length(body.velocity) > tuning.motion.max_speed * stall_speed_fraction) {
        heading_ = kinetra::atan2(body.velocity.y, body.velocity.x);
        stalled_ = std::max(stalled_ - dt, 0.f);
    } else {
        // The target always lies within a narrow cone around the heading, so a body
        // that avoidance stopped or slowed to a crawl in front of terrain or other
        // agents would keep aiming into them. It turns on the spot instead, to the
        // side its wander angle points at, until the way ahead is free.
        heading_ = std::remainder(heading_ + std::copysign(stall_turn_rate * dt, wander_angle_), full_turn);
        stalled_ = std::min(stalled_ + dt, stall_release);
    }
    Vec2 direction = rotated({1, 0}, heading_);
    const Vec2 to_home = home_ - body.position;
    const float away = length(to_home);
    const float leash_start = wander.range * leash_start_fraction;
    if (away > leash_start) {
        // Blend toward home gradually, so the agent turns back in a curve instead of
        // reversing at the edge of its range. The pull fades while the body is
        // stalled: when terrain blocks the straight way home, the agent first walks
        // out of the pocket it is in, and the pull returns as it travels.
        const float pull = std::min((away - leash_start) / (wander.range - leash_start), 1.f) *
                           (1 - stalled_ / stall_release);
        const Vec2 homeward = to_home / away;
        const Vec2 blended = normalized(lerp(direction, homeward, pull));
        // Exactly opposed directions cancel halfway through the blend.
        direction = length(blended) > 0 ? blended : homeward;
    }
    goal_ = body.position + direction * wander.reach + rotated(direction, wander_angle_) * (wander.reach / 2);
    return goal_;
}

void Agent::choose_wander_goal(const kinetra::Body &body, const kinetra::Navigation &navigation,
                               const WanderTuning &wander) {
    route_.clear();
    const kinetra::Vec2 to_home = home_ - body.position;
    const float heading =
        kinetra::length(to_home) > wander.range ? kinetra::atan2(to_home.y, to_home.x) : heading_;
    for (int attempt = 0; attempt < 12; ++attempt) {
        const float turn = (unit() * 2 - 1) * (attempt < 6 ? 0.7f : 3.14159265f);
        const float distance = 4 + unit() * 4;
        auto candidate = body.position + kinetra::rotated({distance, 0}, heading + turn);
        auto path = navigation.path(body.position, candidate);
        if (path.empty() || kinetra::length(path.back() - body.position) < 2)
            continue;
        goal_ = path.back();
        route_ = std::move(path);
        heading_ = heading + turn;
        wander_remaining_ = 0.8f + unit() * 0.8f;
        break;
    }
    if (route_.empty()) {
        goal_ = body.position;
        wander_remaining_ = replan_interval;
    }
    replan_in_ = replan_interval;
}
} // namespace demo
