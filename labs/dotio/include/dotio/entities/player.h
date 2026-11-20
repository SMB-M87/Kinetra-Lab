#ifndef PLAYER_H
#define PLAYER_H

#include "dotio/entities/physics_object.h"
#include "dotio/navigation/grid.h"

namespace dotio {
class World;

class Player final : public PhysicsObject {
  private:
    World *_world{};

  public:
    float Speed{1.0f};
    float Health{100.0f};
    float MaxHealth{100.0f};
    float Mana{100.0f};
    float MaxMana{100.0f};
    float Cooldown{20.0f};
    float MaxCooldown{20.0f};
    float BoltSpeed{10.0f};
    int Kills{1};
    float MaxBulletKills{100.0f};
    dotio::math::Vec2 Heading;

    Player();

    explicit Player(World *world, dotio::math::Position position, float direction);

    ~Player() override = default;

    void Update(float delta) override;

    void Draw(dotio::Renderer &renderer) override;

    void Shoot(const dotio::math::Coordinates mouse, const bool debug);
};
} // namespace dotio

#endif // PLAYER_H
