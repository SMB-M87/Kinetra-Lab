#pragma once

#include "dotio/renderer.h"

namespace dotio {
class StaticObject {
  public:
    bool IsActive;
    dotio::math::Position Position{};
    float Direction{};

    explicit StaticObject(dotio::math::Position position, float direction, bool active);

    virtual ~StaticObject();

    virtual void Draw(dotio::Renderer &renderer);
};
} // namespace dotio
