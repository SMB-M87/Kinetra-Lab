#pragma once

#include "dotio/entities/physics_object.h"
#include <kinetra/simulation/collision_grid.h>
#include <kinetra/simulation/weighted_grid.h>

#include <array>
#include <cstdint>
#include <map>
#include <random>
#include <tuple>

namespace dotio {
/// Occupancy raster of a world's resident window: wall cells for collision, sight lines,
/// projectile sweeps and routes. One instance per World; nonzero cells are walls.
///
/// Cell (row, column) covers `Origin + (column, row) * CellSize` to one cell further, in
/// legacy pixels; +Y points down the screen.
///
/// The grid also owns the route planner, a kinetra::WeightedGrid with one planning cell per
/// raster cell in the same legacy coordinates. Every write goes through SetCell(), which
/// blocks or frees the planning cell too, so routes and collision always see the same walls.
class Grid {
  public:
    static constexpr int rows = 155;
    static constexpr int columns = 155;
    /// Edge length of one square cell, in legacy pixels.
    static constexpr float cell_size = 100.0f;
    static inline const dotio::math::GridSize grid_size{rows, columns};
    /// Legacy position of the corner of cell (0, 0).
    static inline const dotio::math::Coordinates origin{0.0f, 0.0f};
    /// A floor-only grid: every cell 0.
    Grid();

    /// Value of cell (row, column): nonzero for a wall. Both must lie inside the grid.
    int cell(int row, int column) const { return cells_[row][column]; }
    /// Writes cell (row, column), nonzero for a wall, and blocks or frees its planning cell.
    /// Both must lie inside the grid.
    void set_cell(int row, int column, int value);
    /// Writes every cell, as SetCell() does.
    void fill(int value);
    /// The route planner: cell id `row * Columns + column`, positions in legacy pixels.
    const kinetra::WeightedGrid &navigation() const { return navigation_; }

    /// A borrowed view of the raster for Kinetra's grid queries; valid while this grid lives.
    kinetra::CollisionGrid collision_view() const;
    /// Fills the raster with the seeded terrain whose cell (offset_row, offset_column) is cell
    /// (0, 0) of the window.
    void generate(std::uint32_t seed, int offset_row = 0, int offset_column = 0);
    /// Terrain of the absolute cell (row, column) for `seed`: 1 for a wall, 0 for floor.
    /// Independent of the raster; remembers recently generated districts.
    int terrain_cell(std::uint32_t seed, int row, int column) const;

    /// Pushes `object` out of every wall cell it overlaps.
    void handle_grid_collision(dotio::PhysicsObject &object) const;

    static bool segment_is_intersecing_circle(const dotio::math::Segment &segment,
                                              const dotio::math::Point2 c, const float r);

    static uint8_t segement_circle_intersection(const dotio::math::Segment &segment,
                                                const dotio::math::Point2 c, const float r,
                                                dotio::math::Point2 &out_intersection);

    /// Whether `segment` meets a wall cell or leaves the window; `intersection` receives the
    /// first point it does.
    bool is_intersecting_any_cell(dotio::math::Segment segment, dotio::math::Point2 &intersection) const;

    /// A free cell at most five cells from `origin` in each direction, drawn from `random`.
    /// Throws std::runtime_error when there is none.
    dotio::math::GridEntry generate_grid_entry(dotio::math::GridEntry origin, std::mt19937 &random) const;

    /// Center of a free cell at least three cells inside the window, drawn from `random`.
    /// Draws until it finds one; the window must have a free cell there.
    dotio::math::Position generate_position(std::mt19937 &random) const;

  private:
    int cells_[rows][columns]{};
    kinetra::WeightedGrid navigation_;
    /// Generated 32-cell districts by (seed, district row, district column).
    mutable std::map<std::tuple<std::uint32_t, int, int>, std::array<int, 32 * 32>> terrain_cache_;
};
} // namespace dotio
