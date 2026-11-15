#include "physics_object.h"
#include "physics_adapter.h"
#include "portable.h"

namespace logic {
PhysicsObject::~PhysicsObject() = default;

PhysicsObject::PhysicsObject() {
}

PhysicsObject::PhysicsObject(const float mass, const float radius, const float friction,
                             const float rotationalFriction, const gmath::Position position,
                             const float direction, bool active)
    : MovingObject(position, direction, active), _mass(mass), _friction(friction),
      _angularFriction(rotationalFriction), Radius(radius) {
}

void PhysicsObject::ApplyForce(const gmath::Vector2 &force, const float) {
    // Legacy steering supplies a velocity change, not a force in newtons.
    auto body = dotio::physics_body(*this);
    kinetra::apply_body_impulse(body, dotio::physics_vector(force) * body.mass, body.position);
    dotio::apply_body(*this, body);
}

void PhysicsObject::Update(const float delta) {
    auto body = dotio::physics_body(*this);
    kinetra::integrate(body, delta, _friction);
    dotio::apply_body(*this, body);
}

void PhysicsObject::Tick() {
    MovingObject::Tick();
}

float PhysicsObject::GetDiameter() const {
    return 2.0f * Radius;
}

float PhysicsObject::GetFriction() const {
    return _friction;
}
float PhysicsObject::GetMass() const {
    return _mass;
}
} // namespace logic
