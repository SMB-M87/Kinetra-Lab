#pragma once

#include "dotio/entities/static_object.h"

namespace dotio {
class MovingObject : public StaticObject {
  public:
    dotio::math::Vector2 Velocity{};
    float AngularVelocity{};

    explicit MovingObject(dotio::math::Position position, float direction, bool active);

    MovingObject();

    virtual void update(float delta);

    virtual void tick();

    [[nodiscard]] dotio::math::Vector2 get_velocity() const;
    void set_velocity(dotio::math::Vec2 velocity);

    [[nodiscard]] dotio::math::Vector2 get_next_position() const;
};
} // namespace dotio
