#pragma once

#include "dotio/math/gmath.h"

#include <cstddef>
#include <span>
#include <vector>

namespace dotio {
class Grid;

/// Waypoints an enemy follows, in legacy pixels, nearest first. It does not include the
/// point the route was planned from.
class Route {
  public:
    /// An empty route: nothing to follow.
    Route() = default;
    /// Follows `waypoints` in order.
    explicit Route(std::vector<dotio::math::Vec2> waypoints);

    /// Whether no waypoint is left.
    bool empty() const { return next_ >= waypoints_.size(); }
    /// Number of waypoints left, the next one included.
    std::size_t size() const { return waypoints_.size() - next_; }
    /// The waypoint to steer toward; throws std::out_of_range when empty().
    const dotio::math::Vec2 &next() const;
    /// The last waypoint, where the route ends; throws std::out_of_range when empty().
    const dotio::math::Vec2 &destination() const;
    /// The waypoints left, the next one first.
    std::span<const dotio::math::Vec2> remaining() const;
    /// Drops the next waypoint; does nothing when empty().
    void advance();
    /// Drops every waypoint.
    void clear();

  private:
    std::vector<dotio::math::Vec2> waypoints_;
    std::size_t next_{};
};

/// The cheapest route over the free cells of `grid` from `from` to `to`, both in legacy
/// pixels: the centers of the cells between them, then `to` itself. Eight neighbors per cell;
/// a diagonal step needs both cells beside it free. Empty when either point lies in a wall
/// or outside the grid, or no route exists.
Route plan_route(const Grid &grid, dotio::math::Vec2 from, dotio::math::Vec2 to);

/// A route from `from` to a shelter from `player`, both in legacy pixels: the recovery
/// state's search for a place to reload out of sight.
///
/// The search spreads from the enemy's cell in order of route cost, entering only cells on
/// the side of the player it started on (the half-planes through the player's cell that
/// contain the enemy's cell) and more than one cell away from the player. A cell is out of
/// sight when the line from its center to `player` meets a wall. Counting out-of-sight cells
/// in that order, the start's own cell included, the route ends at the center of the first
/// one whose count exceeds `fear`, so a more fearful enemy runs further. Empty when there is
/// no such cell.
Route plan_shelter(const Grid &grid, dotio::math::Vec2 from, dotio::math::Vec2 player, float fear);
} // namespace dotio
