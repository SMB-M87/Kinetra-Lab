#pragma once

#include "tuning.h"

#include <kinetra/navigation.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <span>

namespace sandbox {

/// Sandbox patrol entity referencing a scene-owned physics body by a scene-remapped index.
/// Seeded wander, commanded travel, route, visibility and replanning share one lifecycle.
///
/// An agent wanders around its home until retarget() commands a goal. Commanded travel
/// always follows a planned route. Wandering either steers without a route or plans short
/// routes, as WanderTuning::mode says; both turn back beyond the home range.
class Agent {
  public:
    Agent(std::size_t body_id, kinetra::Vec2 home, std::uint32_t seed = 0)
        : body_id_(body_id), home_(home), goal_(home),
          random_(seed ^ (std::uint32_t(body_id) * 2654435761u)) {
        heading_ = unit() * full_turn;
    }

    std::size_t body_id() const { return body_id_; }
    void remap_body(std::size_t id) { body_id_ = id; }
    /// Where the agent was placed; the center of the range it wanders in.
    kinetra::Vec2 home() const { return home_; }
    /// The point the agent heads for: a commanded goal, the end of its wander route, or the
    /// point it steers at while it wanders without a route.
    kinetra::Vec2 goal() const { return goal_; }
    /// Waypoints still ahead; empty while the agent steers without a route.
    std::span<const kinetra::Vec2> route() const { return route_; }
    bool active() const { return active_; }
    float replan_in() const { return replan_in_; }
    bool wandering() const { return wandering_; }
    /// Whether the last active tick wandered by steering. Such an agent follows no route, so
    /// only local avoidance keeps it clear of terrain, and there is no route to draw. False
    /// during route wander and commanded travel, and for a dormant agent.
    bool steers_without_route() const { return steers_without_route_; }

    /// Keep the displayed route until the next active tick computes a replacement.
    void retarget(kinetra::Vec2 destination) {
        if (!active_)
            return;
        goal_ = destination;
        wandering_ = false;
        command_remaining_ = 20;
        invalidate_route();
    }
    void invalidate_route() { replan_in_ = 0; }
    void update_visibility(kinetra::Aabb view, const kinetra::Body &body) {
        const bool was_active = active_;
        active_ = view.overlaps(kinetra::bounds(body));
        if (!active_) {
            route_.clear();
            steers_without_route_ = false;
        } else if (!was_active)
            invalidate_route();
    }

    /// Advance active patrol/replanning state and consume reached waypoints.
    /// Dormant agents preserve goals and timers but discard routes; failed plans target the current pose.
    ///
    /// Returns the point to steer `body` toward during this tick of `dt` seconds. `tuning`
    /// must pass validate(); it may change between ticks, and a change of wander mode takes
    /// effect on the next wandering tick. Of the motion values only `max_speed` is read, to
    /// tell a body that makes no headway from one that travels.
    kinetra::Vec2 advance_navigation(float dt, const kinetra::Body &body,
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

  private:
    /// Wander without a route (Reynolds' wander behavior): aim at a point on a circle that
    /// lies `reach` ahead along the direction of travel. The point sits on the circle at the
    /// wander angle, which drifts by a seeded random amount every tick, so the course bends
    /// smoothly instead of jumping between goals. Beyond leash_start_fraction of the home
    /// range the direction is blended toward home, fully at the range. A stalled body has no
    /// direction of travel: it uses its stored heading and turns that on the spot. Returns
    /// the point and reports it as goal().
    kinetra::Vec2 steer_wander(float dt, const kinetra::Body &body, const Tuning &tuning) {
        using namespace kinetra;
        const WanderTuning &wander = tuning.wander;
        route_.clear();
        // Idle timers: a switch to route wander plans its first leg at once.
        replan_in_ = wander_remaining_ = 0;
        wander_angle_ = std::remainder(wander_angle_ + (unit() * 2 - 1) * wander.jitter * dt, full_turn);
        if (length(body.velocity) > tuning.motion.max_speed * stall_speed_fraction) {
            heading_ = std::atan2(body.velocity.y, body.velocity.x);
            stalled_ = std::max(stalled_ - dt, 0.f);
        } else {
            // The target always lies within a narrow cone around the heading, so a body
            // that avoidance stopped or slowed to a crawl in front of terrain or other
            // agents would keep aiming into them. It turns on the spot instead, to the
            // side its wander angle points at, until the way ahead is free.
            heading_ =
                std::remainder(heading_ + std::copysign(stall_turn_rate * dt, wander_angle_), full_turn);
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
        goal_ =
            body.position + direction * wander.reach + rotated(direction, wander_angle_) * (wander.reach / 2);
        return goal_;
    }

    /// Evolve a seeded heading through short reachable legs. Bounded steering
    /// acceleration smooths changing targets into curves. Bounded attempts and retries
    /// avoid spinning in enclosed terrain. Beyond the home range a leg starts toward home.
    void choose_wander_goal(const kinetra::Body &body, const kinetra::Navigation &navigation,
                            const WanderTuning &wander) {
        route_.clear();
        const kinetra::Vec2 to_home = home_ - body.position;
        const float heading =
            kinetra::length(to_home) > wander.range ? std::atan2(to_home.y, to_home.x) : heading_;
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
    float unit() { return float(random_() >> 8) / 16777216.f; }
    static constexpr float replan_interval = 2;
    /// One full turn in radians.
    static constexpr float full_turn = 6.283185307f;
    /// Share of the home range inside which steering wander is free; beyond it the agent
    /// is pulled toward home, fully at the range itself.
    static constexpr float leash_start_fraction = 0.6f;
    /// Share of the top speed below which a steering wanderer counts as stalled: a free one
    /// travels at nearly top speed, so this slow it is blocked, and its velocity no longer
    /// says where it wants to go. A larger share frees agents sooner but weakens the pull
    /// toward home in a crowd, where everyone is slow.
    static constexpr float stall_speed_fraction = 0.3f;
    /// Radians per second by which a stalled steering wanderer turns its heading: half a
    /// turn in about 1.5 seconds, slower than a moving agent can turn.
    static constexpr float stall_turn_rate = 2;
    /// Seconds of being stalled after which the pull toward home has faded completely; it
    /// returns over the same time once the body travels again.
    static constexpr float stall_release = 1;
    std::size_t body_id_;
    kinetra::Vec2 home_, goal_;
    std::vector<kinetra::Vec2> route_;
    bool active_{};
    float replan_in_{};
    bool wandering_{true};
    bool steers_without_route_{};
    /// `heading_`: direction in radians of the last wander leg, or of the last travel while
    /// steering. `wander_angle_`: where on the wander circle the agent aims, in radians
    /// relative to its direction of travel. `stalled_`: seconds a steering wanderer was
    /// recently stalled, between zero and stall_release.
    float wander_remaining_{}, heading_{}, command_remaining_{}, wander_angle_{}, stalled_{};
    std::mt19937 random_;
};

} // namespace sandbox
