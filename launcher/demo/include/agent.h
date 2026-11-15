#pragma once

#include <kinetra/navigation.h>
#include <random>
#include <span>

namespace sandbox {

/// Sandbox patrol entity referencing a scene-owned physics body by a scene-remapped index.
/// Seeded wander, commanded travel, route, visibility and replanning share one lifecycle.
class Agent {
  public:
    Agent(std::size_t body_id, kinetra::Vec2 home, std::uint32_t seed = 0)
        : body_id_(body_id), home_(home), goal_(home),
          random_(seed ^ (std::uint32_t(body_id) * 2654435761u)) {
        heading_ = unit() * 6.283185307f;
    }

    std::size_t body_id() const { return body_id_; }
    void remap_body(std::size_t id) { body_id_ = id; }
    kinetra::Vec2 home() const { return home_; }
    kinetra::Vec2 goal() const { return goal_; }
    std::span<const kinetra::Vec2> route() const { return route_; }
    bool active() const { return active_; }
    float replan_in() const { return replan_in_; }
    bool wandering() const { return wandering_; }

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
        if (!active_)
            route_.clear();
        else if (!was_active)
            invalidate_route();
    }

    /// Advance active patrol/replanning state and consume reached waypoints.
    /// Dormant agents preserve goals and timers but discard routes; failed plans target the current pose.
    kinetra::Vec2 advance_navigation(float dt, const kinetra::Body &body, const kinetra::Navigation &navigation) {
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
        if (wandering_ && (wander_remaining_ <= 0 || (route_.empty() && replan_in_ <= 0))) {
            choose_wander_goal(body, navigation);
        } else if (replan_in_ <= 0) {
            route_ = navigation.path(body.position, goal_);
            if (!route_.empty())
                goal_ = route_.back();
            replan_in_ = replan_interval;
        }
        return route_target(body.position, route_, navigation);
    }

  private:
    /// Sample forward-biased destinations around the current pose, accepting only
    /// reachable routes. Bounded attempts and retries avoid spinning in enclosed terrain.
    void choose_wander_goal(const kinetra::Body &body, const kinetra::Navigation &navigation) {
        route_.clear();
        const float heading =
            kinetra::length(body.velocity) > 0.2f ? std::atan2(body.velocity.y, body.velocity.x) : heading_;
        for (int attempt = 0; attempt < 12; ++attempt) {
            const float turn = (unit() * 2 - 1) * (attempt < 6 ? 1.8f : 3.14159265f);
            const float distance = 5 + unit() * 11;
            auto candidate = body.position + kinetra::rotated({distance, 0}, heading + turn);
            auto path = navigation.path(body.position, candidate);
            if (path.empty() || kinetra::length(path.back() - body.position) < 2)
                continue;
            goal_ = path.back();
            route_ = std::move(path);
            heading_ = heading + turn;
            wander_remaining_ = 10 + unit() * 8;
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
    std::size_t body_id_;
    kinetra::Vec2 home_, goal_;
    std::vector<kinetra::Vec2> route_;
    bool active_{};
    float replan_in_{};
    bool wandering_{true};
    float wander_remaining_{}, heading_{}, command_remaining_{};
    std::mt19937 random_;
};

} // namespace sandbox
