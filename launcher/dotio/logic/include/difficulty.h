#pragma once

#include <array>
#include <stdexcept>
#include <string_view>

namespace logic {
enum class Difficulty { easy, medium, hard };

/// Session-owned balancing policy; geometry, player abilities and spawn exclusion are invariant.
struct DifficultySettings {
    std::string_view name;
    int active_spawn_limit;
    float spawn_rate_multiplier, crowd_load_multiplier, spawn_spacing;
    float movement_multiplier, fire_interval_multiplier, projectile_speed, prediction;
    int pursuit_cycles;
    int enemy_hits;
};
inline constexpr std::array difficulty_presets{DifficultySettings{.name = "EASY",
                                                                  .active_spawn_limit = 8,
                                                                  .spawn_rate_multiplier = 1.f,
                                                                  .crowd_load_multiplier = 1.f,
                                                                  .spawn_spacing = 350,
                                                                  .movement_multiplier = 0.85f,
                                                                  .fire_interval_multiplier = 1.35f,
                                                                  .projectile_speed = 11,
                                                                  .prediction = 0.45f,
                                                                  .pursuit_cycles = 6,
                                                                  .enemy_hits = 1},
                                               DifficultySettings{.name = "MEDIUM",
                                                                  .active_spawn_limit = 12,
                                                                  .spawn_rate_multiplier = 1.5f,
                                                                  .crowd_load_multiplier = 0.7f,
                                                                  .spawn_spacing = 325,
                                                                  .movement_multiplier = 1.f,
                                                                  .fire_interval_multiplier = 1.f,
                                                                  .projectile_speed = 13,
                                                                  .prediction = 0.8f,
                                                                  .pursuit_cycles = 10,
                                                                  .enemy_hits = 2},
                                               DifficultySettings{.name = "HARD",
                                                                  .active_spawn_limit = 18,
                                                                  .spawn_rate_multiplier = 2.f,
                                                                  .crowd_load_multiplier = 0.5f,
                                                                  .spawn_spacing = 275,
                                                                  .movement_multiplier = 1.1f,
                                                                  .fire_interval_multiplier = 0.8f,
                                                                  .projectile_speed = 15,
                                                                  .prediction = 1.f,
                                                                  .pursuit_cycles = 16,
                                                                  .enemy_hits = 3}};

inline const DifficultySettings &settings_for(Difficulty difficulty) {
    const auto index = static_cast<std::size_t>(difficulty);
    if (index >= difficulty_presets.size())
        throw std::invalid_argument("Unsupported Dotio difficulty");
    return difficulty_presets[index];
}
inline Difficulty parse_difficulty(std::string_view name) {
    if (name == "easy")
        return Difficulty::easy;
    if (name == "medium")
        return Difficulty::medium;
    if (name == "hard")
        return Difficulty::hard;
    throw std::invalid_argument("Difficulty must be easy, medium or hard");
}
} // namespace logic
