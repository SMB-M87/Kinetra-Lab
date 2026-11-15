#pragma once
#include <gmath.h>

namespace gamef {
/// Drawing boundary retained by the ported game; the runtime adapter owns Y-axis conversion.
class Renderer {
  public:
    virtual ~Renderer() = default;
    virtual void DrawCircle(gmath::Position center, float radius, gmath::RGBA color) = 0;
    virtual void DrawSegment(gmath::Segment segment, gmath::RGBA color) = 0;
};
} // namespace gamef
