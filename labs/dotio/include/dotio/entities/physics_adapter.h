#pragma once
#include "dotio/entities/physics_object.h"
#include <kinetra/simulation/physics.h>

namespace dotio {
/// The same point or vector as a Kinetra vector; no unit changes (legacy pixels stay pixels).
inline kinetra::Vec2 physics_vector(dotio::math::Vec2 value) {
    return {value.x, value.y};
}
/// The same point or vector as a Dotio vector.
inline dotio::math::Vec2 legacy_vector(kinetra::Vec2 value) {
    return {value.x, value.y};
}
/// `object` as a controlled Kinetra circle without friction or restitution, for one
/// collision or integration step. Units stay legacy pixels and updates, which Kinetra's
/// physics accepts as long as they are used consistently.
inline kinetra::Body physics_body(const dotio::PhysicsObject &object) {
    kinetra::Body body;
    body.position = physics_vector(object.position);
    body.velocity = physics_vector(object.velocity);
    body.radius = object.radius;
    body.mass = object.get_mass();
    body.angle = object.direction;
    body.angular_velocity = object.angular_velocity;
    body.motion = kinetra::Motion::controlled;
    body.friction = 0;
    body.restitution = 0;
    return body;
}
/// Writes position, velocity, angle and angular velocity of `body` back into `object`.
inline void apply_body(dotio::PhysicsObject &object, const kinetra::Body &body) {
    object.position = legacy_vector(body.position);
    object.velocity = legacy_vector(body.velocity);
    object.direction = body.angle;
    object.angular_velocity = body.angular_velocity;
}
} // namespace dotio
