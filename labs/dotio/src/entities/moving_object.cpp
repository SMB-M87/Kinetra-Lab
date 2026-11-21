#include "dotio/entities/moving_object.h"
#include "dotio/entities/physics_adapter.h"

namespace dotio {
MovingObject::MovingObject(const dotio::math::Position start, const float start_direction, bool active)
    : StaticObject(start, start_direction, active) {
}

MovingObject::MovingObject() : MovingObject({}, 0.0f, false) {
}

void MovingObject::update(const float delta) {
    kinetra::Body body;
    body.position = dotio::physics_vector(position);
    body.velocity = dotio::physics_vector(velocity);
    body.angle = direction;
    body.angular_velocity = angular_velocity;
    kinetra::integrate(body, delta);
    position = dotio::legacy_vector(body.position);
    direction = body.angle;
}

void MovingObject::tick() {
}
} // namespace dotio
