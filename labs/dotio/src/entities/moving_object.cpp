#include "dotio/entities/moving_object.h"
#include "dotio/entities/physics_adapter.h"

namespace dotio {
MovingObject::MovingObject(const dotio::math::Position position, const float direction, bool active)
    : StaticObject(position, direction, active) {
}

MovingObject::MovingObject() : MovingObject({}, 0.0f, false) {
}

void MovingObject::update(const float delta) {
    kinetra::Body body;
    body.position = dotio::physics_vector(Position);
    body.velocity = dotio::physics_vector(Velocity);
    body.angle = Direction;
    body.angular_velocity = AngularVelocity;
    kinetra::integrate(body, delta);
    Position = dotio::legacy_vector(body.position);
    Direction = body.angle;
}

void MovingObject::tick() {
}

dotio::math::Vector2 MovingObject::get_velocity() const {
    return Velocity;
}
void MovingObject::set_velocity(const dotio::math::Vec2 velocity) {
    Velocity = velocity;
}

dotio::math::Vector2 MovingObject::get_next_position() const {
    return Position + Velocity;
}
} // namespace dotio
