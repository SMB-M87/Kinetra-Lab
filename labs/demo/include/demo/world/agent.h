#pragma once

#include "demo/world/tuning.h"

#include <kinetra/simulation/navigation.h>

#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace demo {

/// Scene patrol entity referencing a scene-owned physics body by a scene-remapped index.
/// Seeded wander, commanded travel, route, visibility and replanning share one lifecycle.
///
/// An agent wanders around its home until retarget() commands a goal. Commanded travel
/// always follows a planned route. Wandering either steers without a route or plans short
/// routes, as WanderTuning::mode says; both turn back beyond the home range.
class Agent {
  public:
    /// An agent for world body `body_id`, wandering around `home`. Its random sequence is
    /// derived from `seed` and the body id, so agents of one scene wander differently but
    /// repeatably. It starts dormant: update_visibility() wakes it.
    Agent(std::size_t body_id, kinetra::Vec2 home, std::uint32_t seed = 0);

    /// World body id of the agent's body, kept current by the scene.
    std::size_t body_id() const { return body_id_; }
    /// Points the agent at body `id` after the scene renumbered the world.
    void remap_body(std::size_t id) { body_id_ = id; }
    /// Where the agent was placed; the center of the range it wanders in.
    kinetra::Vec2 home() const { return home_; }
    /// The point the agent heads for: a commanded goal, the end of its wander route, or the
    /// point it steers at while it wanders without a route.
    kinetra::Vec2 goal() const { return goal_; }
    /// Waypoints still ahead; empty while the agent steers without a route.
    std::span<const kinetra::Vec2> route() const { return route_; }
    /// Whether the agent is awake: its body overlapped the active view at the last
    /// update_visibility(). Dormant agents neither steer nor take commands.
    bool active() const { return active_; }
    /// Simulated seconds until the route is planned again; zero or less means at the next
    /// active tick.
    float replan_in() const { return replan_in_; }
    /// Whether the agent wanders, rather than travelling to a commanded goal.
    bool wandering() const { return wandering_; }
    /// Whether the last active tick wandered by steering. Such an agent follows no route, so
    /// only local avoidance keeps it clear of terrain, and there is no route to draw. False
    /// during route wander and commanded travel, and for a dormant agent.
    bool steers_without_route() const { return steers_without_route_; }

    /// Commands an awake agent to travel to `destination` for at most 20 active simulated
    /// seconds; a dormant agent ignores it. Keeps the displayed route until the next active
    /// tick computes a replacement.
    void retarget(kinetra::Vec2 destination);
    /// Plans the route again at the next active tick, for example after terrain changed.
    void invalidate_route() { replan_in_ = 0; }
    /// Wakes the agent when `body`, its own body, overlaps `view`, and puts it to sleep
    /// otherwise. Falling asleep drops the route; waking up replans it.
    void update_visibility(kinetra::Aabb view, const kinetra::Body &body);

    /// Advance active patrol/replanning state and consume reached waypoints.
    /// Dormant agents preserve goals and timers but discard routes; failed plans target the current pose.
    ///
    /// Returns the point to steer `body` toward during this tick of `dt` seconds. `tuning`
    /// must pass validate(); it may change between ticks, and a change of wander mode takes
    /// effect on the next wandering tick. Of the motion values only `max_speed` is read, to
    /// tell a body that makes no headway from one that travels.
    kinetra::Vec2 advance_navigation(float dt, const kinetra::Body &body,
                                     const kinetra::Navigation &navigation, const Tuning &tuning);

  private:
    /// Wander without a route (Reynolds' wander behavior): aim at a point on a circle that
    /// lies `reach` ahead along the direction of travel. The point sits on the circle at the
    /// wander angle, which drifts by a seeded random amount every tick, so the course bends
    /// smoothly instead of jumping between goals. Beyond leash_start_fraction of the home
    /// range the direction is blended toward home, fully at the range. A stalled body has no
    /// direction of travel: it uses its stored heading and turns that on the spot. Returns
    /// the point and reports it as goal().
    kinetra::Vec2 steer_wander(float dt, const kinetra::Body &body, const Tuning &tuning);

    /// Evolve a seeded heading through short reachable legs. Bounded steering
    /// acceleration smooths changing targets into curves. Bounded attempts and retries
    /// avoid spinning in enclosed terrain. Beyond the home range a leg starts toward home.
    void choose_wander_goal(const kinetra::Body &body, const kinetra::Navigation &navigation,
                            const WanderTuning &wander);
    /// A seeded random value in [0, 1).
    float unit() { return float(random_() >> 8) / 16777216.f; }
    /// Simulated seconds between two plans of the same route.
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

} // namespace demo
