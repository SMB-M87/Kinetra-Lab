#include "dotio/navigation/route.h"

#include "dotio/navigation/grid.h"

#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dotio {
namespace {
kinetra::Vec2 planner_point(dotio::math::Vec2 point) {
    return {point.X, point.Y};
}

Route route_of(const std::vector<kinetra::Vec2> &points) {
    std::vector<dotio::math::Vec2> waypoints;
    waypoints.reserve(points.size());
    for (const auto point : points)
        waypoints.push_back({point.x, point.y});
    return Route(std::move(waypoints));
}

/// Cell of `point`, or {-1, -1} outside the grid.
dotio::math::GridEntry cell_of(dotio::math::Vec2 point) {
    dotio::math::GridEntry cell;
    if (!dotio::math::GridEntry::from_world(point, Grid::origin, Grid::cell_size, Grid::grid_size, cell))
        return {-1, -1};
    return cell;
}

/// Which sides of the player's cell the enemy started on, per axis, with the player's own
/// row or column counting as both sides. The original search kept to these sides.
struct Sides {
    bool up, down, left, right;

    /// Whether a cell at `rows` and `columns` from the player's cell (positive: down, right)
    /// may be entered: on the enemy's side along one axis, the player's line included, and
    /// strictly on its side along the other.
    bool admit(int rows, int columns) const {
        const bool rows_inclusive = (up && rows <= 0) || (down && rows >= 0);
        const bool rows_strict = (up && rows < 0) || (down && rows > 0);
        const bool columns_inclusive = (left && columns <= 0) || (right && columns >= 0);
        const bool columns_strict = (left && columns < 0) || (right && columns > 0);
        return (rows_inclusive && columns_strict) || (columns_inclusive && rows_strict);
    }
};
} // namespace

Route::Route(std::vector<dotio::math::Vec2> waypoints) : waypoints_(std::move(waypoints)) {
}

const dotio::math::Vec2 &Route::next() const {
    if (empty())
        throw std::out_of_range("Empty Dotio route");
    return waypoints_[next_];
}

const dotio::math::Vec2 &Route::destination() const {
    if (empty())
        throw std::out_of_range("Empty Dotio route");
    return waypoints_.back();
}

std::span<const dotio::math::Vec2> Route::remaining() const {
    return std::span<const dotio::math::Vec2>(waypoints_).subspan(empty() ? waypoints_.size() : next_);
}

void Route::advance() {
    if (!empty())
        ++next_;
}

void Route::clear() {
    waypoints_.clear();
    next_ = 0;
}

Route plan_route(const Grid &grid, dotio::math::Vec2 from, dotio::math::Vec2 to) {
    return route_of(grid.navigation().path(planner_point(from), planner_point(to)));
}

Route plan_shelter(const Grid &grid, dotio::math::Vec2 from, dotio::math::Vec2 player, float fear) {
    const auto start = cell_of(from);
    const auto hunter = cell_of(player);
    if (start.Row < 0 || hunter.Row < 0)
        return {};
    const int rows = start.Row - hunter.Row, columns = start.Column - hunter.Column;
    const Sides sides{rows <= 0, rows >= 0, columns <= 0, columns >= 0};
    const auto enter = [&](int id) {
        const int row = id / Grid::columns - hunter.Row, column = id % Grid::columns - hunter.Column;
        // Next to the player is no shelter, and no way past it either.
        return sides.admit(row, column) && (std::abs(row) > 1 || std::abs(column) > 1);
    };
    int hidden = 0;
    const auto accept = [&](int id) {
        const auto center = grid.navigation().center(id);
        dotio::math::Point2 hit{};
        if (!grid.is_intersecting_any_cell({{center.x, center.y}, player}, hit))
            return false;
        // The count includes this cell; a fear of 2.5 takes the third hidden cell.
        return float(++hidden) > fear;
    };
    return route_of(grid.navigation().search(planner_point(from), accept, enter));
}
} // namespace dotio
