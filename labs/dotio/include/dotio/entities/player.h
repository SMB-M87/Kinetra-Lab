#pragma once

#include "dotio/entities/physics_object.h"
#include "dotio/navigation/grid.h"

namespace dotio {
class World;

/// The player: moves along `heading`, regenerates health and mana, and shoots bolts. The
/// World owns it; the Application sets `heading` and calls shoot() from the commands.
class Player final : public PhysicsObject {
  private:
    /// The world bolts are added to; borrowed, it owns this player.
    World *world_{};

  public:
    /// Speed gained per update along a full `heading`, in legacy pixels per update.
    float speed{1.0f};
    /// Health points: an arrow takes 10, a kamikaze contact 30; at zero the world resets.
    /// Regenerates 0.01 per update and is clamped to `max_health`.
    float health{100.0f};
    float max_health{100.0f};
    /// Mana points: a shot costs 10. Regenerates 0.1 per update, clamped to `max_mana`.
    float mana{100.0f};
    float max_mana{100.0f};
    /// Updates until the next shot is allowed; `max_cooldown` after each shot.
    float cooldown{20.0f};
    float max_cooldown{20.0f};
    /// Bolt speed added at `max_bullet_kills` kills, in legacy pixels per update; a bolt starts
    /// at 6 plus this times kills / max_bullet_kills.
    float bolt_speed{10.0f};
    /// Kills plus one: starts at 1 and stops at max_bullet_kills + 1; reset on death.
    int kills{1};
    float max_bullet_kills{100.0f};
    /// Movement input in legacy coordinates (+Y down), each component in [-1, 1].
    dotio::math::Vec2 heading;

    Player();

    explicit Player(World *world, dotio::math::Position position, float direction);

    ~Player() override = default;

    /// Regenerates, counts the cooldown down and accelerates along `heading` over `delta`
    /// updates. Diagonal input is scaled so it is no faster than straight input.
    void update(float delta) override;

    void draw(dotio::Renderer &renderer) override;

    /// Fires a bolt from the player toward `mouse` (legacy pixels) when the cooldown is over
    /// and at least 10 mana is left. With `debug` (invulnerable debug mode) it costs no mana.
    void shoot(const dotio::math::Coordinates mouse, const bool debug);
};
} // namespace dotio
