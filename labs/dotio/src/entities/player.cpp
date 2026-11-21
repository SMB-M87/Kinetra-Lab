#include "dotio/entities/player.h"
#include "dotio/world.h"
#include <cmath>

namespace dotio {
Player::Player() : PhysicsObject() {
}

Player::Player(World *world, dotio::math::Position position, const float direction)
    : PhysicsObject(80.0f, dotio::Grid::cell_size * 0.2, 0.2f, 0.2f, position, direction, true),
      world_(world) {
}

void Player::update(const float delta) {
    Mana += 0.10f * delta;
    if (Mana > MaxMana)
        Mana = MaxMana;

    Health += 0.01f * delta;
    if (Health > MaxHealth)
        Health = MaxHealth;

    Cooldown -= 1.0f * delta;
    if (Cooldown <= 0.0f)
        Cooldown = 0.0f;

    dotio::math::Vec2 normal_heading = Heading;

    normal_heading.X = Heading.X * sqrtf(1.0f - 0.5f * (Heading.Y * Heading.Y));
    normal_heading.Y = Heading.Y * sqrtf(1.0f - 0.5f * (Heading.X * Heading.X));

    apply_force(normal_heading * Speed * delta, 0.5f * get_mass());
    PhysicsObject::update(delta);
}

void Player::draw(dotio::Renderer &renderer) {
    renderer.draw_circle(Position, Radius, {0.4, 0.0, 1.0, 1.0});
}

void Player::shoot(const dotio::math::Coordinates mouse, const bool debug) {
    if (Mana < 10.0f || Cooldown > 0.0f)
        return;

    dotio::math::Vector2 force{(6.0f + (BoltSpeed * (Kills / MaxBulletKills))), 0.0f};
    force.rotate((mouse - Position).get_direction());
    world_->add_bolt(Position, force);
    if (!debug)
        Mana -= 10.0f;
    Cooldown = MaxCooldown;
}
} // namespace dotio
