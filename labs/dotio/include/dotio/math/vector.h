#pragma once

#include <kinetra/simulation/math.h>

namespace dotio::math {
/// A point or vector in legacy coordinates (pixels at 25 per world unit, +Y down the screen).
using Vec2 = kinetra::Vec2;
using Vector2 = Vec2;
using Size2 = Vec2;
using Point2 = Vec2;
using Coord = Vec2;
using Coordinates = Vec2;
using Pos = Vec2;
using Position = Vec2;

/// A color: red, green, blue and opacity, each in [0, 1].
struct RGBA {
    float r, g, b, a;
};

/// A line segment from `start` to `end`.
struct Segment {
    Point2 start;
    Point2 end;
};
} // namespace dotio::math
