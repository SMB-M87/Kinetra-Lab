#ifndef GRID_H
#define GRID_H

#include "dotio/entities/physics_object.h"
#include <kinetra/simulation/collision_grid.h>

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
class Grid {
  public:
    static constexpr int Rows = 155;
    static constexpr int Columns = 155;
    /// Edge length of one square cell, in legacy pixels.
    static constexpr float CellSize = 100.0f;
    static inline const dotio::math::GridSize GridSize{Rows, Columns};
    /// Legacy position of the corner of cell (0, 0).
    static inline const dotio::math::Coordinates Origin{0.0f, 0.0f};
    /// The raster, row by row; nonzero means wall.
    int Array[Rows][Columns]{};

    /// A borrowed view of Array for Kinetra's grid queries; valid while this grid lives.
    kinetra::CollisionGrid CollisionView() const;
    /// Fills Array with the seeded terrain whose cell (offset_row, offset_column) is cell
    /// (0, 0) of the window.
    void Generate(std::uint32_t seed, int offset_row = 0, int offset_column = 0);
    /// Terrain of the absolute cell (row, column) for `seed`: 1 for a wall, 0 for floor.
    /// Independent of Array; remembers recently generated districts.
    int TerrainCell(std::uint32_t seed, int row, int column) const;

    /// Pushes `object` out of every wall cell it overlaps.
    void HandleGridCollision(dotio::PhysicsObject &object) const;

    bool GridIntersection(const dotio::math::Segment &segment, dotio::math::Point2 &intersection) const;

    static bool SegmentIsIntersecingCircle(const dotio::math::Segment &segment, const dotio::math::Point2 c,
                                           const float r);

    static uint8_t SegementCircleIntersection(const dotio::math::Segment &segment,
                                              const dotio::math::Point2 c, const float r,
                                              dotio::math::Point2 &outIntersection);

    /// Whether `segment` meets a wall cell or leaves the window; `intersection` receives the
    /// first point it does.
    bool IsIntersectingAnyCell(dotio::math::Segment segment, dotio::math::Point2 &intersection) const;

    /// A free cell at most five cells from `origin` in each direction, drawn from `random`.
    /// Throws std::runtime_error when there is none.
    dotio::math::GridEntry GenerateGridEntry(dotio::math::GridEntry origin, std::mt19937 &random) const;

    /// Center of a free cell at least three cells inside the window, drawn from `random`.
    /// Draws until it finds one; the window must have a free cell there.
    dotio::math::Position GeneratePosition(std::mt19937 &random) const;

  private:
    /// Generated 32-cell districts by (seed, district row, district column).
    mutable std::map<std::tuple<std::uint32_t, int, int>, std::array<int, 32 * 32>> terrain_cache_;
};
} // namespace dotio
#endif
