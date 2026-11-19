#include "geometry.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
// Slab test: the strict interior keeps boxes that only touch apart.
bool crosses_mover(Vec2 start, Vec2 end, Vec2 extent, const Body &other) {
    const Vec2 low = other.position - other.half_extents - extent;
    const Vec2 high = other.position + other.half_extents + extent;
    const Vec2 travel = end - start;
    float enter = 0, exit = 1;
    for (int axis = 0; axis < 2; ++axis) {
        const float p = axis == 0 ? start.x : start.y;
        const float v = axis == 0 ? travel.x : travel.y;
        const float lo = axis == 0 ? low.x : low.y;
        const float hi = axis == 0 ? high.x : high.y;
        if (v == 0) {
            if (p <= lo || p >= hi)
                return false;
        } else {
            float near = (lo - p) / v, far = (hi - p) / v;
            if (near > far)
                std::swap(near, far);
            enter = std::max(enter, near);
            exit = std::min(exit, far);
            if (enter >= exit)
                return false;
        }
    }
    return true;
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
float support(const Body &body, Vec2 direction) {
    return std::abs(direction.x) * body.half_extents.x + std::abs(direction.y) * body.half_extents.y;
}
Aabb travel_bounds(Vec2 from, Vec2 to, Vec2 extent) {
    return {{std::min(from.x, to.x) - extent.x, std::min(from.y, to.y) - extent.y},
            {std::max(from.x, to.x) + extent.x, std::max(from.y, to.y) + extent.y}};
}
} // namespace i4sim::detail
