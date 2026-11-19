#include "dotio/difficulty.h"

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace dotio {
namespace {
/// Upper limit of a value that has none.
constexpr float unbounded = std::numeric_limits<float>::infinity();

/// Throws unless `value` is finite and lies in [minimum, maximum]; `name` goes in the message.
void require_range(float value, float minimum, float maximum, std::string_view name) {
    if (std::isfinite(value) && value >= minimum && value <= maximum)
        return;
    std::ostringstream message;
    message << "Dotio " << name << " must be finite and at least " << minimum;
    if (std::isfinite(maximum))
        message << " and at most " << maximum;
    throw std::invalid_argument(message.str());
}

/// Throws unless `value` is at least `minimum`; `name` goes in the message.
void require_at_least(int value, int minimum, std::string_view name) {
    if (value >= minimum)
        return;
    std::ostringstream message;
    message << "Dotio " << name << " must be at least " << minimum;
    throw std::invalid_argument(message.str());
}
} // namespace

void validate(const DifficultySettings &settings) {
    require_at_least(settings.active_spawn_limit, 0, "active spawn limit");
    require_range(settings.spawn_rate_multiplier, 0, unbounded, "spawn rate multiplier");
    require_range(settings.crowd_load_multiplier, 0, unbounded, "crowd load multiplier");
    require_range(settings.spawn_spacing, 0, unbounded, "spawn spacing");
    require_range(settings.movement_multiplier, 0, max_movement_multiplier, "movement multiplier");
    require_range(settings.fire_interval_multiplier, 0, unbounded, "fire interval multiplier");
    // The only value with an open limit: aiming divides by the speed, so zero is out too.
    if (!std::isfinite(settings.projectile_speed) || settings.projectile_speed <= 0)
        throw std::invalid_argument("Dotio projectile speed must be finite and above zero");
    require_range(settings.prediction, 0, unbounded, "prediction");
    require_at_least(settings.pursuit_cycles, 1, "pursuit cycles");
    require_at_least(settings.enemy_hits, 1, "enemy hits");
}

void validate(const EnemyTuning &tuning) {
    if (tuning.wander != WanderMode::steering && tuning.wander != WanderMode::path)
        throw std::invalid_argument("Dotio wander mode must be steering or path");
    require_range(tuning.wander_force, 0, max_state_force, "wander force");
    require_range(tuning.attack_force, 0, max_state_force, "attack force");
    require_range(tuning.pursue_force, 0, max_state_force, "pursue force");
    require_range(tuning.recover_force, 0, max_state_force, "recover force");
    require_range(tuning.kamikaze_force, 0, max_state_force, "kamikaze force");
}
} // namespace dotio
