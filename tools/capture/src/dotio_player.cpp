#include "capture/dotio_player.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace capture {
namespace {
/// Whether `enemy` is alive, simulated and with its center on screen for `camera`, at least
/// `margin` logical pixels away from every edge of the window.
bool on_screen(const dotio::Enemy &enemy, const kinetra::Camera2D &camera, float margin = 0) {
    if (!enemy.IsActive || enemy.Frozen)
        return false;
    const auto point = camera.world_to_screen(dotio::to_world(enemy.Position));
    return point.x >= margin && point.y >= margin && point.x <= camera.viewport.x - margin &&
           point.y <= camera.viewport.y - margin;
}

/// Whether a bolt flies from `from` to `to` without meeting a wall cell or a crate, which
/// stops a bolt that comes within the bolt's radius of it (World::UpdateBolts).
bool clear_shot(const dotio::World &world, dotio::math::Vec2 from, dotio::math::Vec2 to) {
    dotio::math::Point2 hit{};
    if (world.grid().is_intersecting_any_cell({from, to}, hit))
        return false;
    for (const auto &crate : world.get_objects())
        if (kinetra::segment_circle(dotio::physics_vector(from), dotio::physics_vector(to),
                                    crate.body.position, crate.body.radius + dotio::Bolt::radius))
            return false;
    return true;
}
} // namespace

float dotio_bolt_speed(const dotio::Player &player) {
    return 6.0f + player.BoltSpeed * (float(player.Kills) / player.MaxBulletKills);
}

dotio::Enemy *dotio_target(dotio::Application &application, const kinetra::Camera2D &camera) {
    auto &world = application.world();
    const auto player = world.get_player().Position;
    dotio::Enemy *nearest = nullptr;
    float nearest_distance = std::numeric_limits<float>::infinity();
    for (auto &enemy : world.get_enemies()) {
        if (!on_screen(enemy, camera))
            continue;
        const float distance = (enemy.Position - player).get_length();
        if (distance < nearest_distance && clear_shot(world, player, enemy.Position)) {
            nearest = &enemy;
            nearest_distance = distance;
        }
    }
    return nearest;
}

dotio::math::Vec2 lead_offset(dotio::math::Vec2 offset, dotio::math::Vec2 velocity, float bolt_speed) {
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
    const auto &player = application.world().get_player();
    const auto point = player.Position + lead_offset(enemy->Position - player.Position, enemy->Velocity,
                                                     dotio_bolt_speed(player));
    const auto screen = camera.world_to_screen(dotio::to_world(point));
    // A pointer stays inside its window, also when the lead runs off screen.
    return kinetra::Vec2{std::clamp(screen.x, 0.f, camera.viewport.x),
                         std::clamp(screen.y, 0.f, camera.viewport.y)};
}

bool dotio_recovery_in_view(dotio::Application &application, const kinetra::Camera2D &camera, float margin) {
    for (auto &enemy : application.world().get_enemies())
        if (on_screen(enemy, camera, margin) && enemy.is_recovering())
            return true;
    return false;
}
} // namespace capture
