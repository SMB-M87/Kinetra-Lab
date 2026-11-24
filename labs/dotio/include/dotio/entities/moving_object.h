#pragma once

#include "dotio/entities/static_object.h"

namespace dotio {
/// An object that moves every update. Time is counted in updates: one update is 1/60 s.
class MovingObject : public StaticObject {
  public:
    /// Legacy pixels per update.
    kinetra::Vec2 velocity{};
    /// Radians per update.
    float angular_velocity{};

    explicit MovingObject(kinetra::Vec2 position, float direction, bool active);

    /// An inactive object at the origin.
    MovingObject();

    /// Moves by `velocity` over `delta` updates (1 at the 60 Hz step), without friction.
    virtual void update(float delta);

    /// The decision tick (20 Hz, every third update); the base decides nothing.
    virtual void tick();
};
} // namespace dotio
