#include "dotio/entities/moving_object.h"
#include "dotio/entities/physics_adapter.h"

namespace dotio {
MovingObject::MovingObject(const kinetra::Vec2 start, const float start_direction, bool active)
    : StaticObject(start, start_direction, active) {
}

MovingObject::MovingObject() : MovingObject({}, 0.0f, false) {
}

void MovingObject::update(const float delta) {
    kinetra::Body body;
    body.position = position;
    body.velocity = velocity;
    body.angle = direction;
    body.angular_velocity = angular_velocity;
    kinetra::integrate(body, delta);
    position = body.position;
    direction = body.angle;
}

void MovingObject::tick() {
}
} // namespace dotio
