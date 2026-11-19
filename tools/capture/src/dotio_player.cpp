#include "dotio_player.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace capture {
namespace {
/// Whether `enemy` is alive, simulated and with its center on screen for `camera`, at least
/// `margin` logical pixels away from every edge of the window.
bool on_screen(const logic::Enemy &enemy, const kinetra::Camera2D &camera, float margin = 0) {
    if (!enemy.IsActive || enemy.Frozen)
        return false;
    const auto point = camera.world_to_screen(dotio::to_world(enemy.Position));
    return point.x >= margin && point.y >= margin && point.x <= camera.viewport.x - margin &&
           point.y <= camera.viewport.y - margin;
}

/// Whether no wall cell lies between `from` and `to`.
bool clear_line(gmath::Vec2 from, gmath::Vec2 to) {
    gmath::Point2 hit{};
    return !logic::Grid::IsIntersectingAnyCell({from, to}, &logic::Grid::Array[0][0], hit);
}

/// Speed of the bolt the player fires now, in legacy pixels per tick (Player::Shoot).
float bolt_speed(const logic::Player &player) {
    return 6.0f + player.BoltSpeed * (float(player.Kills) / player.MaxBulletKills);
}
} // namespace

logic::Enemy *dotio_target(dotio::Application &application, const kinetra::Camera2D &camera) {
    auto &world = application.world();
    const auto player = world.GetPlayer().Position;
    logic::Enemy *nearest = nullptr;
    float nearest_distance = std::numeric_limits<float>::infinity();
    for (auto &enemy : world.GetEnemies()) {
        if (!on_screen(enemy, camera))
            continue;
        const float distance = (enemy.Position - player).GetLength();
        if (distance < nearest_distance && clear_line(player, enemy.Position)) {
            nearest = &enemy;
            nearest_distance = distance;
        }
    }
    return nearest;
}

gmath::Vec2 lead_offset(gmath::Vec2 offset, gmath::Vec2 velocity, float bolt_speed) {
    // |offset + velocity t| = bolt_speed t, solved for the earliest t > 0.
    const float a = velocity.X * velocity.X + velocity.Y * velocity.Y - bolt_speed * bolt_speed;
    const float b = 2 * (offset.X * velocity.X + offset.Y * velocity.Y);
    const float c = offset.X * offset.X + offset.Y * offset.Y;
    float time = -1;
    if (std::abs(a) < 1e-4f) {
        // Equal speeds: the quadratic term vanishes and the equation is linear.
        if (b < 0)
            time = -c / b;
    } else if (const float discriminant = b * b - 4 * a * c; discriminant >= 0) {
        const float root = std::sqrt(discriminant);
        const float first = (-b - root) / (2 * a), second = (-b + root) / (2 * a);
        time = std::min(first, second) > 0 ? std::min(first, second) : std::max(first, second);
    }
    if (!(time > 0) || time > longest_lead_ticks)
        return offset;
    return offset + velocity * time;
}

std::optional<kinetra::Vec2> dotio_aim(dotio::Application &application, const kinetra::Camera2D &camera) {
    const auto *enemy = dotio_target(application, camera);
    if (!enemy)
        return std::nullopt;
    const auto &player = application.world().GetPlayer();
    const auto point =
        player.Position + lead_offset(enemy->Position - player.Position, enemy->Velocity, bolt_speed(player));
    const auto screen = camera.world_to_screen(dotio::to_world(point));
    // A pointer stays inside its window, also when the lead runs off screen.
    return kinetra::Vec2{std::clamp(screen.x, 0.f, camera.viewport.x),
                         std::clamp(screen.y, 0.f, camera.viewport.y)};
}

bool dotio_recovery_in_view(dotio::Application &application, const kinetra::Camera2D &camera, float margin) {
    for (auto &enemy : application.world().GetEnemies())
        if (on_screen(enemy, camera, margin) && enemy.IsRecovering())
            return true;
    return false;
}
} // namespace capture
