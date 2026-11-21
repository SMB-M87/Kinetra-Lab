#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace dotio {
enum class Difficulty { easy, medium, hard };

/// Session-owned balancing policy; geometry, player abilities and spawn exclusion are invariant.
struct DifficultySettings {
    std::string_view name;
    int active_spawn_limit;
    float spawn_rate_multiplier, crowd_load_multiplier, spawn_spacing;
    float movement_multiplier, fire_interval_multiplier, projectile_speed, prediction;
    int pursuit_cycles;
    int enemy_hits;
    /// Whether every value matches, for example a tuned copy and its preset.
    friend bool operator==(const DifficultySettings &, const DifficultySettings &) = default;
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

/// How an enemy moves while it is in its WANDER state.
enum class WanderMode {
    /// Follow a drifting heading and turn away from walls with feelers; no path is planned
    /// (see wander.h).
    steering,
    /// Follow an A* path to a random free cell nearby, as the original game did.
    path,
};

/// Largest DifficultySettings::movement_multiplier validate() accepts.
///
/// An enemy gains `force * movement_multiplier` legacy pixels per update of speed and loses
/// a fifth of its speed per update, so its speed settles at four times that product. With
/// both values at their limits that is 48 pixels per update, below half a cell (50). Wall
/// contact is tested after the move and pushes a body out through the nearest face of the
/// cell, so an enemy that moved further than half a cell into a wall would leave it on the
/// far side.
inline constexpr float max_movement_multiplier = 4.f;
/// Largest state force validate() accepts; see max_movement_multiplier for the reason.
inline constexpr float max_state_force = 3.f;

/// Enemy behavior a session can change while it runs, next to its DifficultySettings.
/// The defaults are the values of the original game.
///
/// A force is the speed an enemy gains per update in that state, in legacy pixels per
/// update, before DifficultySettings::movement_multiplier scales it. validate() states the
/// accepted values.
struct EnemyTuning {
    /// How the WANDER state moves.
    WanderMode wander{WanderMode::steering};
    /// WANDER: along the wander heading, or toward the next path cell in WanderMode::path.
    float wander_force{0.75f};
    /// ATTACK: toward the player, or away from it inside the enemy's fear distance.
    float attack_force{0.85f};
    /// PURSUE: toward the next cell of the path to the player.
    float pursue_force{0.85f};
    /// RECOVER: toward the next cell of the path to shelter.
    float recover_force{1.1f};
    /// KAMIKAZE: straight at a visible player, otherwise along the path to it.
    float kamikaze_force{1.3f};
    /// Whether the mode and every force match, for example a tuned copy and the defaults.
    friend bool operator==(const EnemyTuning &, const EnemyTuning &) = default;
};

/// Throws std::invalid_argument, naming the value, unless a world can run with `settings`.
///
/// Every number must be finite. The limits follow from how World and Enemy read each value:
/// - `active_spawn_limit` >= 0: spawning stops while that many enemies are near the player.
/// - `spawn_rate_multiplier`, `crowd_load_multiplier` >= 0: they scale the spawn rate and
///   the load Fuzzy::SpawnRate accepts, which rejects a negative load.
/// - `spawn_spacing` >= 0: a distance in legacy pixels between a spawn and other enemies.
/// - `movement_multiplier` in [0, max_movement_multiplier]: zero stops the enemies, a
///   negative value would make them steer backwards.
/// - `fire_interval_multiplier` >= 0: scales the seconds between shots; zero fires at
///   every decision tick.
/// - `projectile_speed` > 0, in legacy pixels per update: aiming divides the player's speed
///   by it.
/// - `prediction` >= 0: the part of the lead angle an enemy aims ahead of a moving player.
/// - `pursuit_cycles` >= 1 and `enemy_hits` >= 1: an enemy pursues for at least one cycle
///   and takes at least one hit.
/// `name` is not checked.
void validate(const DifficultySettings &settings);
/// Throws std::invalid_argument, naming the value, unless enemies can run with `tuning`:
/// `wander` must be a WanderMode enumerator and every force finite and in
/// [0, max_state_force]. A force of zero makes the state stand still.
void validate(const EnemyTuning &tuning);
} // namespace dotio
