#pragma once
#include "dotio/math/gmath.h"

namespace dotio {
/// Drawing boundary retained by the ported game; the runtime adapter owns Y-axis conversion.
class Renderer {
  public:
    virtual ~Renderer() = default;
    virtual void DrawCircle(dotio::math::Position center, float radius, dotio::math::RGBA color) = 0;
    virtual void DrawSegment(dotio::math::Segment segment, dotio::math::RGBA color) = 0;
};
} // namespace dotio
