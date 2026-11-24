#pragma once
#include <kinetra/simulation/math.h>
#include <kinetra/view/input.h>

namespace dotio {
/// Drawing boundary retained by the ported game; the runtime adapter owns Y-axis conversion.
class Renderer {
  public:
    virtual ~Renderer() = default;
    /// A filled circle around `center`, radius in legacy pixels.
    virtual void draw_circle(kinetra::Vec2 center, float radius, kinetra::Color color) = 0;
    /// A line along `segment`, in legacy pixels.
    virtual void draw_segment(kinetra::Segment segment, kinetra::Color color) = 0;
};
} // namespace dotio
