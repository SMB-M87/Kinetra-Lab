#include "dotio/entities/physics_object.h"
#include "dotio/entities/physics_adapter.h"

namespace dotio {
PhysicsObject::~PhysicsObject() = default;

PhysicsObject::PhysicsObject() {
}

PhysicsObject::PhysicsObject(const float mass, const float radius, const float friction,
                             const float rotational_friction, const dotio::math::Position position,
                             const float direction, bool active)
    : MovingObject(position, direction, active), _mass(mass), _friction(friction),
      _angularFriction(rotational_friction), Radius(radius) {
}

void PhysicsObject::apply_force(const dotio::math::Vector2 &force, const float) {
    // Legacy steering supplies a velocity change, not a force in newtons.
    auto body = dotio::physics_body(*this);
    kinetra::apply_body_impulse(body, dotio::physics_vector(force) * body.mass, body.position);
    dotio::apply_body(*this, body);
}

void PhysicsObject::update(const float delta) {
    auto body = dotio::physics_body(*this);
    kinetra::integrate(body, delta, _friction);
    dotio::apply_body(*this, body);
}

void PhysicsObject::tick() {
    MovingObject::tick();
}

float PhysicsObject::get_diameter() const {
    return 2.0f * Radius;
}

float PhysicsObject::get_friction() const {
    return _friction;
}
float PhysicsObject::get_mass() const {
    return _mass;
}
} // namespace dotio
