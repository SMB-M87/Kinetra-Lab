#pragma once

#include "dotio/math/vector.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace dotio::math {
/// A grid cell (row, column), or a grid size when used as GridSize. Rows grow down the
/// screen. {-1, -1} (null_grid_entry) marks no cell.
typedef struct GridEntry {
  public:
    int row;
    int column;

    /// Cell (0, 0).
    GridEntry();
    /// A square size: `size` rows and `size` columns.
    explicit GridEntry(const int size);
    GridEntry(const int row, const int column);
    /// Legacy position of the corner of cell (row, column).
    static Coordinates to_world(const int row, const int column, const float cell_size,
                                const Coordinates &origin);
    /// Whether `entry` lies inside a grid of `size`.
    static bool in_bounds(const GridEntry &entry, const GridEntry &size);
    /// Stores the cell containing `position` in `output` and returns whether it lies inside a
    /// grid of `grid_size`; `output` is set either way.
    static bool from_world(const Position &position, const Coordinates &origin, const float cell_size,
                           const GridEntry &grid_size, GridEntry &output);
    /// Position of `position` inside its cell, from the cell's corner.
    static Position get_cell_position(const Position &position, const Coordinates &origin,
                                      const float cell_size);
    /// Bit mask of the neighboring cells a square of half width `semi_width` around
    /// `position` reaches into (left or right, up or down, and the diagonal between them).
    static unsigned char get_square_partition(const Position &position, float semi_width,
                                              const Coordinates &origin, float cell_size);
    /// The cells of `partition` for `position`: its own cell in `output[0]`, the horizontal,
    /// vertical and diagonal neighbor in `output[1..3]` when the mask names them and they lie
    /// in the grid. Leaves the other entries as they were; nothing when `position` is outside.
    static void get_partitions(uint8_t partition, const Position &position, const Coordinates &origin,
                               float cell_size, const GridEntry &grid_size, GridEntry output[4]);
    bool operator==(const GridEntry &other) const;
    /// {-1, -1}: no cell.
    static GridEntry null_grid_entry;
} GridSize, GridPosition, GridCoordinates;
} // namespace dotio::math

namespace std {
template <> struct hash<dotio::math::GridEntry> {
    std::size_t operator()(const dotio::math::GridEntry &entry) const noexcept {
        std::hash<float> hasher;
        std::size_t hash_value = 0;
        hash_value ^= hasher(float(entry.row)) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
        hash_value ^= hasher(float(entry.column)) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
        return hash_value;
    }
};
} // namespace std
