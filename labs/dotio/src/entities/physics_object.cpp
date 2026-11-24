#include "dotio/entities/physics_object.h"
#include "dotio/entities/physics_adapter.h"

namespace dotio {
PhysicsObject::~PhysicsObject() = default;

PhysicsObject::PhysicsObject() {
}

PhysicsObject::PhysicsObject(const float mass, const float body_radius, const float friction,
                             const float rotational_friction, const dotio::math::Position start,
                             const float start_direction, bool active)
    : MovingObject(start, start_direction, active), mass_(mass), friction_(friction),
      angular_friction_(rotational_friction), radius(body_radius) {
}

void PhysicsObject::apply_force(const dotio::math::Vector2 &force, const float) {
    // Legacy steering supplies a velocity change, not a force in newtons.
    auto body = dotio::physics_body(*this);
    kinetra::apply_body_impulse(body, force * body.mass, body.position);
    dotio::apply_body(*this, body);
}

void PhysicsObject::update(const float delta) {
    auto body = dotio::physics_body(*this);
    kinetra::integrate(body, delta, friction_);
    dotio::apply_body(*this, body);
}

void PhysicsObject::tick() {
    MovingObject::tick();
}

float PhysicsObject::get_diameter() const {
    return 2.0f * radius;
}

float PhysicsObject::get_friction() const {
    return friction_;
}
float PhysicsObject::get_mass() const {
    return mass_;
}
} // namespace dotio
