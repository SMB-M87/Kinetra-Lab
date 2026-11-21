#pragma once

#include "dotio/math/gmath.h"

#include "dotio/entities/moving_object.h"

namespace dotio {
class PhysicsObject : public MovingObject {
  protected:
    float mass_;
    float friction_;
    float angular_friction_;

  public:
    float radius;
    dotio::math::GridEntry last_partition[4]{{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};

    PhysicsObject();

    explicit PhysicsObject(float mass, float radius, float friction, float rotational_friction,
                           dotio::math::Position position, float direction, bool active);

    virtual ~PhysicsObject();

    void apply_force(const dotio::math::Vector2 &force, float mass);

    void update(float delta) override;

    void tick() override;

    float get_diameter() const;

    [[nodiscard]] float get_friction() const;

    [[nodiscard]] float get_mass() const;
};
} // namespace dotio
