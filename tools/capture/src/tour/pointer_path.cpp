#include "capture/tour/pointer_path.h"

#include <algorithm>
#include <cmath>

namespace capture {
namespace {
using kinetra::Vec2;

/// Fitts' law for a hand on a mouse: seconds before the move starts to count, seconds per
/// doubling of the distance, and the distance in pixels that counts as one unit.
constexpr double reaction = 0.2, per_doubling = 0.11, unit_distance = 24;

/// Uniform value in [-1, 1) from 24 raw bits, so a seed gives the same paths with every
/// standard library.
float signed_unit(std::mt19937 &random) {
    return static_cast<float>(random() >> 8) / 8388608.f - 1.f;
}
} // namespace

double minimum_jerk(double u) {
    const double t = std::clamp(u, 0.0, 1.0);
    return t * t * t * (10 + t * (6 * t - 15));
}

Vec2 PointerPath::at(double time) const {
    if (duration <= 0 || time >= duration)
        return to;
    if (time <= 0)
        return from;
    const auto s = static_cast<float>(minimum_jerk(time / duration));
    // Quadratic Bezier curve through the control point.
    return from * ((1 - s) * (1 - s)) + control * (2 * s * (1 - s)) + to * (s * s);
}

PointerPath plan_pointer_path(Vec2 from, Vec2 to, std::mt19937 &random) {
    const Vec2 way = to - from;
    const float distance = kinetra::length(way);
    // The bend is drawn even for a move of no length, so the moves after it do not depend
    // on whether an earlier one was needed.
    const float bend = signed_unit(random) * largest_bend;
    if (distance == 0)
        return {from, to, from, 0};
    const Vec2 sideways = kinetra::perpendicular(way / distance) * (distance * bend);
    const double seconds =
        reaction + per_doubling * std::log2(static_cast<double>(distance) / unit_distance + 1);
    return {from, to, (from + to) / 2 + sideways, std::clamp(seconds, shortest_move, longest_move)};
}
} // namespace capture
