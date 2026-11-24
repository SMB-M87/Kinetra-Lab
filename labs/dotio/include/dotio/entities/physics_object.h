#pragma once

#include "dotio/math/cell.h"

#include "dotio/entities/moving_object.h"

namespace dotio {
/// A circular body with mass and friction: the player and the enemies. It reaches Kinetra's
/// physics through physics_adapter.h.
class PhysicsObject : public MovingObject {
  protected:
    /// Mass for contact response between bodies; no unit beyond its ratio to other masses.
    float mass_;
    /// Fraction of its speed the body loses per update: 0.2 settles a constant push at five
    /// times the speed it adds per update.
    float friction_;
    /// Kept from the original game; nothing reads it.
    float angular_friction_;

  public:
    /// Radius of the body, in legacy pixels.
    float radius;
    /// Cells of World::enemy_partition_map this body is registered in, {-1, -1} for none.
    /// Only enemies use them, to find the neighbors they push apart from.
    dotio::math::GridEntry last_partition[4]{{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};

    PhysicsObject();

    explicit PhysicsObject(float mass, float radius, float friction, float rotational_friction,
                           kinetra::Vec2 position, float direction, bool active);

    virtual ~PhysicsObject();

    /// Adds `force` to the velocity: despite the name it is a change of speed, in legacy
    /// pixels per update. The second argument is ignored; the original game passed half the
    /// mass.
    void apply_force(const kinetra::Vec2 &force, float mass);

    /// Moves over `delta` updates and then loses `friction_` of its speed per update.
    void update(float delta) override;

    void tick() override;

    /// Twice the radius, in legacy pixels.
    float get_diameter() const;

    [[nodiscard]] float get_friction() const;

    [[nodiscard]] float get_mass() const;
};
} // namespace dotio
