#pragma once
#include "physics_object.h"
#include <kinetra/physics.h>

namespace dotio {
inline kinetra::Vec2 physics_vector(gmath::Vec2 value) {
    return {value.X, value.Y};
}
inline gmath::Vec2 legacy_vector(kinetra::Vec2 value) {
    return {value.x, value.y};
}
/// Legacy pixels and frame time remain coherent units at this adapter boundary.
inline kinetra::Body physics_body(const logic::PhysicsObject &object) {
    kinetra::Body body;
    body.position = physics_vector(object.Position);
    body.velocity = physics_vector(object.Velocity);
    body.radius = object.Radius;
    body.mass = object.GetMass();
    body.angle = object.Direction;
    body.angular_velocity = object.AngularVelocity;
    body.motion = kinetra::Motion::controlled;
    body.friction = 0;
    body.restitution = 0;
    return body;
}
inline void apply_body(logic::PhysicsObject &object, const kinetra::Body &body) {
    object.Position = legacy_vector(body.position);
    object.Velocity = legacy_vector(body.velocity);
    object.Direction = body.angle;
    object.AngularVelocity = body.angular_velocity;
}
} // namespace dotio
