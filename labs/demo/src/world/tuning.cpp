#include "demo/world/tuning.h"

#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace demo {
void validate(const Tuning &tuning) {
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
