#pragma once

#include <cmath>
#include <initializer_list>
#include <stdexcept>

/// Values of the Demo scene that can change while it runs: how movers accelerate and avoid
/// each other, and how agents wander. The defaults are the scene's former literals, so an
/// untouched Tuning reproduces the scene as it was before the values became adjustable.
namespace demo {

/// How an agent without a commanded goal roams around its home.
enum class WanderMode {
    /// Steer toward a point on a circle ahead of the agent; no route is planned.
    steering,
    /// Plan one short navigation route after another.
    route,
};

/// Distance from its target, in world units, at which a mover starts to slow down.
inline constexpr float slow_radius = 1;
/// Distance from its target, in world units, at which a mover stands still. Together with
/// slow_radius it shapes arrival, which is not tuned: a stop radius at or above the slow
/// radius is invalid for the steering code.
inline constexpr float stop_radius = 0.08f;

/// Motion limits and local avoidance of the agents and of a player that follows a route, in
/// world units and seconds.
struct MotionTuning {
    /// Top speed in units per second.
    float max_speed{4};
    /// Largest acceleration in units per second squared.
    float max_acceleration{12};
    /// Seconds a mover takes to reach the velocity it wants.
    float response_time{0.25f};
    /// Legacy steering only: seconds of travel that are checked for obstacles.
    float lookahead{0.8f};
    /// Extra gap kept from other bodies, in world units.
    float clearance{0.05f};
    /// Legacy steering of agents only: personal space in world units; zero switches
    /// separation off.
    float separation_radius{1.6f};
    /// Legacy steering of agents only: how hard a neighbor at zero distance pushes an agent
    /// away, as a fraction of max_acceleration; the push fades out toward separation_radius.
    float separation_weight{0.65f};
    /// Reciprocal avoidance only: seconds ahead in which conflicts are predicted.
    float horizon{1.5f};
    /// Reciprocal avoidance only: fraction of the wanted speed added toward the right-hand
    /// passing side near neighbors ahead; zero switches the preference off.
    float passing_bias{0.05f};
};

/// How agents wander, in world units, seconds and radians.
struct WanderTuning {
    /// Steering (default) or planned routes; commanded travel always plans a route.
    WanderMode mode{WanderMode::steering};
    /// Home range: the distance from its home beyond which an agent turns back.
    float range{12};
    /// Steering wander only: distance of the wander circle ahead of the agent. The circle's
    /// radius is half of it, so the agent always aims at a point ahead of itself.
    float reach{3};
    /// Steering wander only: largest drift of the wander angle in radians per second. Zero
    /// keeps an agent on a fixed arc; larger values make its course more erratic.
    float jitter{4};
};

/// Everything a debug menu can change in a running scene.
struct Tuning {
    MotionTuning motion;
    WanderTuning wander;
};

/// Throws std::invalid_argument unless every value is finite, `max_speed`,
/// `max_acceleration`, `response_time`, `horizon`, `range` and `reach` are positive, the
/// other values are not negative, and `mode` is one of the WanderMode enumerators.
inline void validate(const Tuning &tuning) {
    const MotionTuning &motion = tuning.motion;
    const WanderTuning &wander = tuning.wander;
    for (const float value : {motion.max_speed, motion.max_acceleration, motion.response_time, motion.horizon,
                              wander.range, wander.reach})
        if (!std::isfinite(value) || value <= 0)
            throw std::invalid_argument("Demo tuning: speed, acceleration, response time, horizon, home "
                                        "range and wander reach must be finite and positive");
    for (const float value : {motion.lookahead, motion.clearance, motion.separation_radius,
                              motion.separation_weight, motion.passing_bias, wander.jitter})
        if (!std::isfinite(value) || value < 0)
            throw std::invalid_argument("Demo tuning: lookahead, clearance, separation, passing bias and "
                                        "wander jitter must be finite and not negative");
    if (wander.mode != WanderMode::steering && wander.mode != WanderMode::route)
        throw std::invalid_argument("Demo tuning: unknown wander mode");
}

} // namespace demo
