#pragma once

#include "dotio/entities/physics_object.h"
#include "dotio/navigation/grid.h"

namespace dotio {
class World;

class Player final : public PhysicsObject {
  private:
    World *world_{};

  public:
    float speed{1.0f};
    float health{100.0f};
    float max_health{100.0f};
    float mana{100.0f};
    float max_mana{100.0f};
    float cooldown{20.0f};
    float max_cooldown{20.0f};
    float bolt_speed{10.0f};
    int kills{1};
    float max_bullet_kills{100.0f};
    dotio::math::Vec2 heading;

    Player();

    explicit Player(World *world, dotio::math::Position position, float direction);

    ~Player() override = default;

    void update(float delta) override;

    void draw(dotio::Renderer &renderer) override;

    void shoot(const dotio::math::Coordinates mouse, const bool debug);
};
} // namespace dotio
