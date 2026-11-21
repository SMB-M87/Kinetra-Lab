#pragma once

#include "dotio/renderer.h"

namespace dotio {
class StaticObject {
  public:
    bool is_active;
    dotio::math::Position position{};
    float direction{};

    explicit StaticObject(dotio::math::Position position, float direction, bool active);

    virtual ~StaticObject();

    virtual void draw(dotio::Renderer &renderer);
};
} // namespace dotio
