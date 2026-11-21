#include "dotio/ai/wander.h"
#include "dotio/entities/enemy.h"
#include "dotio/math/random.h"
#include "dotio/navigation/grid.h"
#include "dotio/world.h"

#include <array>
#include <cmath>
#include <numbers>
#include <random>

namespace dotio {
namespace {
constexpr float pi = std::numbers::pi_v<float>;

/// Unit vector of a heading in legacy coordinates.
dotio::math::Vec2 direction(float heading) {
    return {std::cos(heading), std::sin(heading)};
}

/// Keeps a heading in [-pi, pi], so it stays precise however long an enemy wanders.
float wrapped(float heading) {
    return std::remainder(heading, 2 * pi);
}

/// The feelers of a body at `position` that follows `heading`; the geometry behind both
/// WanderFeelers() and the directions a blocked enemy tries.
std::array<dotio::math::Segment, 3> feelers_at(dotio::math::Position position, float heading) {
    const auto feeler = [&](float angle, float cells) {
        return dotio::math::Segment{position, position + direction(angle) * (cells * Grid::cell_size)};
    };
    return {{feeler(heading, wander_forward_feeler_cells),
             feeler(heading + wander_side_feeler_angle_rad, wander_side_feeler_cells),
             feeler(heading - wander_side_feeler_angle_rad, wander_side_feeler_cells)}};
}

bool blocked(const Grid &grid, const dotio::math::Segment &feeler) {
    dotio::math::Point2 hit{};
    return grid.is_intersecting_any_cell(feeler, hit);
}

/// Uniform value in [-1, 1) from one draw (see math/random.h).
float signed_unit(std::mt19937 &random) {
    return dotio::math::unit(random) * 2.f - 1.f;
}

/// Turns `enemy` to the first direction whose forward feeler is free. The side that was free
/// becomes the preferred one, so the next blocked update continues the same way round
/// instead of turning back into the wall it just left.
void turn_to_free_direction(Enemy &enemy) {
    const auto free_at = [&](float heading) {
        return !blocked(enemy.get_world().grid(), feelers_at(enemy.position, heading)[wander_forward_feeler]);
    };
    for (const float turn : wander_fan_rad)
        for (const float side : {enemy.wander_side, -enemy.wander_side})
            if (free_at(enemy.wander_heading + side * turn)) {
                enemy.wander_heading += side * turn;
                enemy.wander_side = side;
                return;
            }
    if (free_at(enemy.wander_heading + pi))
        enemy.wander_heading += pi;
    // Otherwise the enemy is walled in closer than a feeler on every side. It keeps its
    // heading; the grid collision holds it and a later drift may find a way out.
}
} // namespace

std::array<dotio::math::Segment, 3> wander_feelers(const Enemy &enemy) {
    return feelers_at(enemy.position, enemy.wander_heading);
}

void randomize_wander(Enemy &enemy) {
    auto &random = enemy.get_world().wander_random();
    enemy.wander_heading = signed_unit(random) * pi;
    enemy.wander_side = signed_unit(random) < 0 ? -1.f : 1.f;
}

void drift_wander_heading(Enemy &enemy) {
    enemy.wander_heading = wrapped(enemy.wander_heading +
                                   signed_unit(enemy.get_world().wander_random()) * wander_drift_limit_rad);
}

void steer_wander(Enemy &enemy, const float delta) {
    // A path planned while WanderMode::path was selected must not be drawn or followed later.
    enemy.path.clear();
    const auto feelers = wander_feelers(enemy);
    const auto &grid = enemy.get_world().grid();
    if (blocked(grid, feelers[wander_forward_feeler])) {
        turn_to_free_direction(enemy);
    } else {
        const bool positive = blocked(grid, feelers[wander_positive_feeler]);
        const bool negative = blocked(grid, feelers[wander_negative_feeler]);
        // Both blocked means a passage narrower than the feelers spread: turning either way
        // would aim at a wall, so the forward feeler alone decides there.
        if (positive != negative) {
            enemy.wander_side = positive ? -1.f : 1.f;
            enemy.wander_heading += enemy.wander_side * wander_side_turn_rad_per_update * delta;
        }
    }
    enemy.wander_heading = wrapped(enemy.wander_heading);
    auto &world = enemy.get_world();
    const dotio::math::Vec2 force = direction(enemy.wander_heading) * world.enemy_tuning().wander_force;
    enemy.apply_force(force * (delta * world.difficulty_settings().movement_multiplier),
                      0.5f * enemy.get_mass());
}
} // namespace dotio
