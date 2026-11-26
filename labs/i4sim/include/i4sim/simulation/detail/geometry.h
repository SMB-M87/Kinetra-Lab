#pragma once
// Geometry helpers shared by the i4sim simulation components. All positions and extents
// are in millimeters in blueprint coordinates.
#include <kinetra/simulation/math.h>
#include <kinetra/simulation/physics.h>

#include <vector>

namespace i4sim::detail {
/// Length of the polyline from `position` through every point of `route`, in order.
float route_remaining(kinetra::Vec2 position, const std::vector<kinetra::Vec2> &route);

/// The side of `area` that faces `body`, as a stop line: the side across the axis on which
/// the body is farther from the area.
kinetra::Segment stop_before(const kinetra::Body &body, kinetra::Aabb area);

} // namespace i4sim::detail
