#pragma once

#include <kinetra/simulation/math.h>

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

/// How the Lab applications draw a PATHS layer without hiding the bodies under it. Headless:
/// only Kinetra's plain math.
///
/// Each application draws every route before its moving bodies, so no route hides a player
/// or an agent, and draws the start of a body's own route, route_start(), right after that
/// body and before the next one. The start shows whose route it is; a neighbor drawn later
/// covers it again where the two bodies overlap.
namespace kinetra_lab {

/// Width in screen pixels of a route line of a PATHS layer, the same in every Lab
/// application and at every zoom. Divide by the camera's pixels per unit for a world width.
inline constexpr float route_line_pixels = 2.5f;

/// The part of the polyline from `start` through `route` that stays within `radius` of
/// `start`: `start`, then every waypoint while the route is still inside, then the point
/// where the route first leaves the circle. A route that comes back into the circle later
/// is not followed further.
///
/// Draw a line between each pair of consecutive points. One point (nothing to draw) when
/// `route` is empty or `radius` is not positive.
inline std::vector<kinetra::Vec2> route_start(kinetra::Vec2 start, std::span<const kinetra::Vec2> route,
                                              float radius) {
    std::vector<kinetra::Vec2> points{start};
    if (!(radius > 0))
        return points;
    kinetra::Vec2 from = start;
    for (const auto waypoint : route) {
        const auto offset = waypoint - start;
        if (kinetra::dot(offset, offset) <= radius * radius) {
            points.push_back(waypoint);
            from = waypoint;
            continue;
        }
        // `from` lies inside the circle and `waypoint` outside, so the segment crosses it once,
        // at the larger root of |from + t * step - start| = radius, with t in (0, 1].
        const auto step = waypoint - from;
        const auto inside = from - start;
        const float a = kinetra::dot(step, step);
        const float b = kinetra::dot(inside, step);
        const float c = kinetra::dot(inside, inside) - radius * radius;
        const float t = (-b + std::sqrt(std::max(0.f, b * b - a * c))) / a;
        points.push_back(from + step * std::clamp(t, 0.f, 1.f));
        break;
    }
    return points;
}

} // namespace kinetra_lab
