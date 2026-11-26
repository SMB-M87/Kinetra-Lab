#include "i4sim/simulation/detail/geometry.h"

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
    if (gap.x >= gap.y)
        start.x = end.x = offset.x > 0 ? area.max.x : area.min.x;
    else
        start.y = end.y = offset.y > 0 ? area.max.y : area.min.y;
    return {start, end};
}
} // namespace i4sim::detail
