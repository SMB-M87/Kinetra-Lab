#pragma once

#include "gmath.h"

#include <array>
#include <cstddef>

namespace logic {
class Enemy;

// Steering wander: how an enemy roams in its WANDER state under WanderMode::steering.
// It follows Enemy::WanderHeading, which drifts at random on every decision tick, and turns
// away from walls that its three feelers touch on every update. No path is planned.
// Angles are radians in legacy coordinates: a heading h points along (cos h, sin h), and
// because +Y points down the screen, a positive turn is clockwise on screen.

/// Largest drift of the wander heading per decision tick (20 Hz), to either side, in radians.
inline constexpr float wander_drift_limit_rad = 0.35f;
/// Length of the forward feeler in grid cells; it reaches 0.7 cells beyond the enemy's body.
inline constexpr float wander_forward_feeler_cells = 0.9f;
/// Length of each side feeler in grid cells.
inline constexpr float wander_side_feeler_cells = 0.6f;
/// Angle between the wander heading and each side feeler, in radians.
inline constexpr float wander_side_feeler_angle_rad = 0.6f;
/// Turn away from a single blocked side feeler, in radians per update at the 60 Hz step.
/// Three updates turn further than one decision tick can drift, so drift cannot hold an
/// enemy against a wall.
inline constexpr float wander_side_turn_rad_per_update = 0.15f;
/// Turns tried in this order when the forward feeler is blocked, each to the enemy's
/// preferred side first and then to the other, in radians. A half turn comes last.
inline constexpr std::array<float, 3> wander_fan_rad{0.8f, 1.6f, 2.4f};

/// Positions in the array WanderFeelers() returns: the forward feeler, and the side feelers
/// at the heading plus and minus wander_side_feeler_angle_rad.
inline constexpr std::size_t wander_forward_feeler = 0, wander_positive_feeler = 1,
                             wander_negative_feeler = 2;

/// The three feelers steering wander tests for `enemy`, as segments in legacy coordinates.
///
/// Each starts at the enemy's position and follows its wander heading; the order is given by
/// wander_forward_feeler, wander_positive_feeler and wander_negative_feeler. A feeler is
/// blocked when Grid::IsIntersectingAnyCell reports a hit for it. SteerWander() tests exactly
/// these segments, so a drawing of them shows what the logic sees. Reads the enemy's
/// position and heading and Grid::CellSize; changes nothing.
std::array<gmath::Segment, 3> WanderFeelers(const Enemy &enemy);

/// Gives `enemy` a random wander heading and preferred turning side from its world's
/// World::wander_random(). Enemy::Initialize() and Enemy::Reset() call it; the enemy must
/// belong to a world.
void RandomizeWander(Enemy &enemy);

/// Decision tick of steering wander: turns the heading of `enemy` by a random amount within
/// wander_drift_limit_rad, drawn from its world's World::wander_random().
void DriftWanderHeading(Enemy &enemy);

/// Update of steering wander for `enemy` over `delta` updates (1 at the 60 Hz step).
///
/// A blocked forward feeler turns the heading to the first free direction of wander_fan_rad
/// and then a half turn, and makes the side that was free the preferred one; when every
/// direction is blocked the heading stays. Otherwise one blocked side feeler turns the
/// heading away from it and prefers that side; two blocked side feelers change nothing.
/// The enemy then gains speed along its heading: EnemyTuning::wander_force scaled by `delta`
/// and DifficultySettings::movement_multiplier. Clears Enemy::Path.
void SteerWander(Enemy &enemy, float delta);
} // namespace logic
