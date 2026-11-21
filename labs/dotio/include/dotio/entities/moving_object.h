#pragma once

#include "dotio/entities/static_object.h"

namespace dotio {
class MovingObject : public StaticObject {
  public:
    dotio::math::Vector2 velocity{};
    float angular_velocity{};

    explicit MovingObject(dotio::math::Position position, float direction, bool active);

    MovingObject();

    virtual void update(float delta);

    virtual void tick();
};
} // namespace dotio
