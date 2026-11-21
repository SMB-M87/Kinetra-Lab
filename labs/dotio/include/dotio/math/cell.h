#pragma once

#include "dotio/math/vector.h"

#include <cstddef>
#include <functional>

namespace dotio::math {
typedef struct GridEntry {
  public:
    int row;
    int column;

    GridEntry();
    explicit GridEntry(const int size);
    GridEntry(const int row, const int column);
    static Coordinates to_world(const int row, const int column, const float cell_size,
                                const Coordinates &origin);
    static bool in_bounds(const GridEntry &entry, const GridEntry &size);
    static bool from_world(const Position &position, const Coordinates &origin, const float cell_size,
                           const GridEntry &grid_size, GridEntry &output);
    static Position get_cell_position(const Position &position, const Coordinates &origin,
                                      const float cell_size);
    static unsigned char get_square_partition(const Position &position, float semi_width,
                                              const Coordinates &origin, float cell_size);
    static void get_partitions(uint8_t partition, const Position &position, const Coordinates &origin,
                               float cell_size, const GridEntry &grid_size, GridEntry output[4]);
    bool operator==(const GridEntry &other) const;
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
