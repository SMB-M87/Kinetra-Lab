#pragma once

#include "dotio/renderer.h"

namespace dotio {
/// Base of everything the world places: the player, enemies and projectiles. Positions are
/// legacy pixels of the resident window (100 per grid cell, +Y down the screen).
class StaticObject {
  public:
    /// Whether this object is alive. Pool slots that are not in use are inactive.
    bool is_active;
    /// Center, in legacy pixels.
    dotio::math::Position position{};
    /// Orientation in radians; kept for the physics adapter, no rule reads it.
    float direction{};

    explicit StaticObject(dotio::math::Position position, float direction, bool active);

    virtual ~StaticObject();

    /// Draws the object through `renderer`; the base draws nothing.
    virtual void draw(dotio::Renderer &renderer);
};
} // namespace dotio
