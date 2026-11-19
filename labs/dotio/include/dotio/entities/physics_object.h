#ifndef PHYSICS_OBJECT_H
#define PHYSICS_OBJECT_H

#include "dotio/math/gmath.h"

#include "dotio/entities/moving_object.h"

namespace dotio {
class PhysicsObject : public MovingObject {
  protected:
    float _mass;
    float _friction;
    float _angularFriction;

  public:
    float Radius;
    dotio::math::GridEntry LastPartition[4]{{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};

    PhysicsObject();

    explicit PhysicsObject(float mass, float radius, float friction, float rotationalFriction,
                           dotio::math::Position position, float direction, bool active);

    virtual ~PhysicsObject();

    void ApplyForce(const dotio::math::Vector2 &force, float mass);

    void Update(float delta) override;

    void Tick() override;

    float GetDiameter() const;

    [[nodiscard]] float GetFriction() const;

    [[nodiscard]] float GetMass() const;
};
} // namespace dotio

#endif // PHYSICS_OBJECT_H
