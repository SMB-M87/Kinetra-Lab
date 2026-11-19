#pragma once
#include "fleet.h"
#include "floor_plan.h"

#include <cstddef>
#include <vector>

namespace i4sim::detail {
/// Plans a grid route for `mover` to `destination`; empty when there is none.
///
/// A plain grid route is returned unless the mover departs (`require_departure`), returns
/// to its own parking place, or has made no progress for `obstruction_ticks`. Then the
/// route is checked against every stationary or obstructed mover and, when one is in the
/// way, refined through the corners around those movers. With `require_departure`, a route
/// that cannot be cleared is empty; otherwise the plain grid route is kept.
///
/// With `motion.path_planning` off, a grid route still decides whether the destination is
/// reachable, but the route returned is the destination alone: the mover steers straight
/// at it.
std::vector<Vec2> plan_route(const FloorPlan &floor, const Fleet &fleet, const MotionSettings &motion,
                             std::size_t mover, Vec2 destination, bool require_departure = false);

/// Replaces the route of `mover` with a Navigation route around blocked cells and all other
/// movers, when one reaches the destination, and marks it as a detour. Resets the progress
/// measurement either way.
void recover_route(const FloorPlan &floor, Fleet &fleet, std::size_t mover);
} // namespace i4sim::detail
