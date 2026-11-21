#pragma once
#include "dotio/math/vector.h"

namespace dotio {
/// Drawing boundary retained by the ported game; the runtime adapter owns Y-axis conversion.
class Renderer {
  public:
    virtual ~Renderer() = default;
    /// A filled circle around `center`, radius in legacy pixels.
    virtual void draw_circle(dotio::math::Position center, float radius, dotio::math::RGBA color) = 0;
    /// A line along `segment`, in legacy pixels.
    virtual void draw_segment(dotio::math::Segment segment, dotio::math::RGBA color) = 0;
};
} // namespace dotio
