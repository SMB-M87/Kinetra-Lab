#ifndef GRID_H
#define GRID_H

#include "dotio/entities/physics_object.h"
#include <kinetra/simulation/collision_grid.h>

namespace dotio {
/// Streamed local world window, character clearance, visibility and swept projectile geometry.
class Grid {
  public:
    static const int Rows = 155;
    static const int Columns = 155;
    static const dotio::math::GridSize GridSize;
    static dotio::math::Coordinates Origin;
    static float CellSize;
    static int Array[Rows][Columns];
    static kinetra::CollisionGrid CollisionView(const int *cells = &Array[0][0]);
    static void Generate(std::uint32_t seed, int offset_row = 0, int offset_column = 0);
    static int TerrainCell(std::uint32_t seed, int row, int column);

    static void HandleGridCollision(dotio::PhysicsObject &object, const float delta, const int *Array);

    static bool GridIntersection(const dotio::math::Segment &segment, const int *Array,
                                 dotio::math::Point2 &intersection);

    static bool SegmentIsIntersecingCircle(const dotio::math::Segment &segment, const dotio::math::Point2 c,
                                           const float r);

    static uint8_t SegementCircleIntersection(const dotio::math::Segment &segment,
                                              const dotio::math::Point2 c, const float r,
                                              dotio::math::Point2 &outIntersection);

    static bool IsIntersectingAnyCell(dotio::math::Segment segment, const int *Array,
                                      dotio::math::Point2 &intersection);

    static dotio::math::GridEntry GenerateGridEntry(dotio::math::GridEntry origin);

    static dotio::math::Position GeneratePosition();
};
} // namespace dotio
#endif
