#include "dotio/entities/player.h"
#include "dotio/world.h"
#include <cmath>

namespace dotio {
Player::Player() : PhysicsObject() {
}

Player::Player(World *world, dotio::math::Position start, const float start_direction)
    : PhysicsObject(80.0f, dotio::Grid::cell_size * 0.2, 0.2f, 0.2f, start, start_direction, true),
      world_(world) {
}

void Player::update(const float delta) {
    mana += 0.10f * delta;
    if (mana > max_mana)
        mana = max_mana;

    health += 0.01f * delta;
    if (health > max_health)
        health = max_health;

    cooldown -= 1.0f * delta;
    if (cooldown <= 0.0f)
        cooldown = 0.0f;

    dotio::math::Vec2 normal_heading = heading;

    normal_heading.x = heading.x * sqrtf(1.0f - 0.5f * (heading.y * heading.y));
    normal_heading.y = heading.y * sqrtf(1.0f - 0.5f * (heading.x * heading.x));

    apply_force(normal_heading * speed * delta, 0.5f * get_mass());
    PhysicsObject::update(delta);
}

void Player::draw(dotio::Renderer &renderer) {
    renderer.draw_circle(position, radius, {0.4f, 0.0f, 1.0f, 1.0f});
}

void Player::shoot(const dotio::math::Coordinates mouse, const bool debug) {
    if (mana < 10.0f || cooldown > 0.0f)
        return;

    dotio::math::Vector2 force{(6.0f + (bolt_speed * (static_cast<float>(kills) / max_bullet_kills))), 0.0f};
    force.rotate((mouse - position).get_direction());
    world_->add_bolt(position, force);
    if (!debug)
        mana -= 10.0f;
    cooldown = max_cooldown;
}
} // namespace dotio
