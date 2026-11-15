#include "moving_object.h"
#include "physics_adapter.h"
#include "portable.h"

namespace logic {
MovingObject::MovingObject(const gmath::Position position, const float direction, bool active)
    : StaticObject(position, direction, active) {
}

MovingObject::MovingObject() : MovingObject({}, 0.0f, false) {
}

void MovingObject::Update(const float delta) {
    kinetra::Body body;
    body.position = dotio::physics_vector(Position);
    body.velocity = dotio::physics_vector(Velocity);
    body.angle = Direction;
    body.angular_velocity = AngularVelocity;
    kinetra::integrate(body, delta);
    Position = dotio::legacy_vector(body.position);
    Direction = body.angle;
}

void MovingObject::Tick() {
}

gmath::Vector2 MovingObject::GetVelocity() const {
    return Velocity;
}
void MovingObject::SetVelocity(const gmath::Vec2 velocity) {
    Velocity = velocity;
}

gmath::Vector2 MovingObject::GetNextPosition() const {
    return Position + Velocity;
}
} // namespace logic
