#pragma once
// What the scripted player of the Dotio captures sees: the enemy to shoot at and where to
// aim, read from the world of the running session.
#include "dotio/application.h"

#include <kinetra/view/camera.h>

#include <optional>

namespace capture {
/// The enemy the player shoots at: the nearest live enemy that is on screen for `camera`
/// and that a bolt can reach in a straight line. No wall cell lies between it and the
/// player, the sight line the enemies' own AI uses, and no crate, because crates stop
/// bolts. Null when there is no such enemy. The pointer is valid until the session advances.
dotio::Enemy *dotio_target(dotio::Application &application, const kinetra::Camera2D &camera);

/// Where a bolt fired now at `bolt_speed` meets a target at `offset` from the shooter that
/// moves on at `velocity`, relative to the shooter. Lengths in legacy pixels, speeds in
/// legacy pixels per tick. Returns `offset` when the bolt cannot catch the target or would
/// need more than `longest_lead_ticks`, so the shot goes at the target itself.
dotio::math::Vec2 lead_offset(dotio::math::Vec2 offset, dotio::math::Vec2 velocity, float bolt_speed);

/// Speed of the bolt `player` fires now, in legacy pixels per tick: it grows with the
/// player's kills, as in dotio::Player::Shoot.
float dotio_bolt_speed(const dotio::Player &player);

/// Longest flight a shot is led for, in ticks of 1/60 s: beyond it the enemy has turned
/// long before the bolt arrives.
inline constexpr float longest_lead_ticks = 90;

/// The screen point, in logical window pixels inside `camera`'s viewport, the player aims at
/// to hit dotio_target() with a bolt fired now; empty when no enemy is in sight. Read it
/// between two advances of the session, when the camera is on the player.
std::optional<kinetra::Vec2> dotio_aim(dotio::Application &application, const kinetra::Camera2D &camera);

/// Whether an enemy in its Recover state (out of ammunition and on its way to shelter, or
/// reloading there) is on screen for `camera`, at least `margin` logical pixels away from
/// every edge of the window. A margin keeps an enemy that is about to leave the view from
/// counting.
bool dotio_recovery_in_view(dotio::Application &application, const kinetra::Camera2D &camera, float margin);
} // namespace capture
