#include "dotio/ai/wander.h"
#include "dotio/entities/enemy.h"
#include "dotio/math/random.h"
#include "dotio/navigation/grid.h"
#include "dotio/world.h"

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
        return dotio::math::Segment{position, position + direction(angle) * (cells * Grid::CellSize)};
    };
    return {{feeler(heading, wander_forward_feeler_cells),
             feeler(heading + wander_side_feeler_angle_rad, wander_side_feeler_cells),
             feeler(heading - wander_side_feeler_angle_rad, wander_side_feeler_cells)}};
}

bool blocked(const Grid &grid, const dotio::math::Segment &feeler) {
    dotio::math::Point2 hit{};
    return grid.IsIntersectingAnyCell(feeler, hit);
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
        return !blocked(enemy.GetWorld().grid(), feelers_at(enemy.Position, heading)[wander_forward_feeler]);
    };
    for (const float turn : wander_fan_rad)
        for (const float side : {enemy.WanderSide, -enemy.WanderSide})
            if (free_at(enemy.WanderHeading + side * turn)) {
                enemy.WanderHeading += side * turn;
                enemy.WanderSide = side;
                return;
            }
    if (free_at(enemy.WanderHeading + pi))
        enemy.WanderHeading += pi;
    // Otherwise the enemy is walled in closer than a feeler on every side. It keeps its
    // heading; the grid collision holds it and a later drift may find a way out.
}
} // namespace

std::array<dotio::math::Segment, 3> WanderFeelers(const Enemy &enemy) {
    return feelers_at(enemy.Position, enemy.WanderHeading);
}

void RandomizeWander(Enemy &enemy) {
    auto &random = enemy.GetWorld().wander_random();
    enemy.WanderHeading = signed_unit(random) * pi;
    enemy.WanderSide = signed_unit(random) < 0 ? -1.f : 1.f;
}

void DriftWanderHeading(Enemy &enemy) {
    enemy.WanderHeading =
        wrapped(enemy.WanderHeading + signed_unit(enemy.GetWorld().wander_random()) * wander_drift_limit_rad);
}

void SteerWander(Enemy &enemy, const float delta) {
    // A path planned while WanderMode::path was selected must not be drawn or followed later.
    enemy.Path.clear();
    const auto feelers = WanderFeelers(enemy);
    const auto &grid = enemy.GetWorld().grid();
    if (blocked(grid, feelers[wander_forward_feeler])) {
        turn_to_free_direction(enemy);
    } else {
        const bool positive = blocked(grid, feelers[wander_positive_feeler]);
        const bool negative = blocked(grid, feelers[wander_negative_feeler]);
        // Both blocked means a passage narrower than the feelers spread: turning either way
        // would aim at a wall, so the forward feeler alone decides there.
        if (positive != negative) {
            enemy.WanderSide = positive ? -1.f : 1.f;
            enemy.WanderHeading += enemy.WanderSide * wander_side_turn_rad_per_update * delta;
        }
    }
    enemy.WanderHeading = wrapped(enemy.WanderHeading);
    auto &world = enemy.GetWorld();
    const dotio::math::Vec2 force = direction(enemy.WanderHeading) * world.enemy_tuning().wander_force;
    enemy.ApplyForce(force * (delta * world.difficulty_settings().movement_multiplier),
                     0.5f * enemy.GetMass());
}
} // namespace dotio
