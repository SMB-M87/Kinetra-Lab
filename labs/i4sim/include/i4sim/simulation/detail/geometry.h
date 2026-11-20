#pragma once
// Geometry helpers shared by the i4sim simulation components. All positions and extents
// are in millimeters in blueprint coordinates.
#include <kinetra/simulation/math.h>
#include <kinetra/simulation/physics.h>

#include <vector>

namespace i4sim::detail {
/// Length of the polyline from `position` through every point of `route`, in order.
float route_remaining(kinetra::Vec2 position, const std::vector<kinetra::Vec2> &route);

/// True when a box with half extents `extent`, moved from `start` to `end`, passes through
/// the strict interior of `other` grown by `extent` (the Minkowski box of both). Touching
/// the boundary does not count. With `start == end` it tests whether the box overlaps.
bool crosses_mover(kinetra::Vec2 start, kinetra::Vec2 end, kinetra::Vec2 extent, const kinetra::Body &other);

/// The side of `area` that faces `body`, as a stop line: the side across the axis on which
/// the body is farther from the area.
kinetra::Segment stop_before(const kinetra::Body &body, kinetra::Aabb area);

/// Half the extent of `body` along `direction`, for a unit `direction`.
float support(const kinetra::Body &body, kinetra::Vec2 direction);

/// Bounding box of a box with half extents `extent` moving from `from` to `to`.
kinetra::Aabb travel_bounds(kinetra::Vec2 from, kinetra::Vec2 to, kinetra::Vec2 extent);
} // namespace i4sim::detail
