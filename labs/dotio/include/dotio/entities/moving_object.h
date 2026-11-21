#pragma once

#include "dotio/entities/static_object.h"

namespace dotio {
class MovingObject : public StaticObject {
  public:
    dotio::math::Vector2 Velocity{};
    float AngularVelocity{};

    explicit MovingObject(dotio::math::Position position, float direction, bool active);

    MovingObject();

    virtual void Update(float delta);

    virtual void Tick();

    [[nodiscard]] dotio::math::Vector2 GetVelocity() const;
    void SetVelocity(dotio::math::Vec2 velocity);

    [[nodiscard]] dotio::math::Vector2 GetNextPosition() const;
};
} // namespace dotio
