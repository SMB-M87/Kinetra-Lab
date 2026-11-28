#include "i4sim/simulation/detail/geometry.h"

#include <algorithm>
#include <cmath>

namespace i4sim::detail {
using namespace kinetra;
float route_remaining(Vec2 position, const std::vector<Vec2> &route) {
    float remaining = 0;
    for (const auto point : route) {
        remaining += distance(position, point);
        position = point;
    }
    return remaining;
}
Segment stop_before(const Body &body, Aabb area) {
    const Vec2 center = (area.min + area.max) / 2;
    const Vec2 half = (area.max - area.min) / 2;
    const Vec2 offset = body.position - center;
    const Vec2 gap{std::abs(offset.x) - half.x - body.half_extents.x,
                   std::abs(offset.y) - half.y - body.half_extents.y};
    Vec2 start = area.min, end = area.max;
    // A line across the body would make constrain_borders push it out at up to full speed,
    // whatever stands beside it. A body that already reaches past the side stops where it
    // is: the line moves to its front face, so it only brakes.
    if (gap.x >= gap.y)
        start.x = end.x = offset.x > 0 ? std::min(area.max.x, body.position.x - body.half_extents.x)
                                       : std::max(area.min.x, body.position.x + body.half_extents.x);
    else
        start.y = end.y = offset.y > 0 ? std::min(area.max.y, body.position.y - body.half_extents.y)
                                       : std::max(area.min.y, body.position.y + body.half_extents.y);
    return {start, end};
}
} // namespace i4sim::detail
