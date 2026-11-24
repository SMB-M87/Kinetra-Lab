#include "dotio/math/cell.h"

#include <cmath>
#include <cstdint>

namespace dotio::math {
GridEntry::GridEntry() : GridEntry(0) {
}
GridEntry::GridEntry(const int cell_row, const int cell_column) : row(cell_row), column(cell_column) {
}
GridEntry::GridEntry(const int size) : GridEntry(size, size) {
}
kinetra::Vec2 GridEntry::to_world(const int row, const int column, const float cell_size,
                                  const kinetra::Vec2 &origin) {
    return {(static_cast<float>(column) * cell_size) + origin.x,
            (static_cast<float>(row) * cell_size) + origin.y};
}

bool GridEntry::in_bounds(const GridEntry &entry, const GridEntry &size) {
    return !(entry.row >= size.row || entry.column >= size.column || entry.row < 0 || entry.column < 0);
}

bool GridEntry::from_world(const kinetra::Vec2 &position, const kinetra::Vec2 &origin, const float cell_size,
                           const GridEntry &grid_size, GridEntry &output) {
    // clang-format off
        const GridEntry temp = {
            static_cast<int>(floorf((position.y - origin.y) / cell_size)),
            static_cast<int>(floorf((position.x - origin.x) / cell_size))
        };
    // clang-format on

    output = temp;

    return in_bounds(temp, grid_size);
}

kinetra::Vec2 GridEntry::get_cell_position(const kinetra::Vec2 &position, const kinetra::Vec2 &origin,
                                           const float cell_size) {
    const kinetra::Vec2 local = position - origin;
    return {std::fmod(local.x, cell_size), std::fmod(local.y, cell_size)};
}

uint8_t GridEntry::get_square_partition(const kinetra::Vec2 &position, const float semi_width,
                                        const kinetra::Vec2 &origin, float cell_size) {
    const kinetra::Vec2 cell_position = get_cell_position(position, origin, cell_size);
    uint8_t result{};

    if (cell_position.x > cell_size - semi_width) {
        result |= 0b00000001;
    }

    else if (cell_position.x < semi_width) {
        result |= 0b00000011;
    }

    if (cell_position.y > cell_size - semi_width) {
        result |= 0b0000100;

        if (cell_position.x >= cell_size - semi_width) {
            result |= 0b00010000;
        }

        else if (cell_position.x < semi_width) {
            result |= 0b00110000;
        }
    }

    else if (cell_position.y < semi_width) {
        result |= 0b00001100;

        if (cell_position.x > cell_size - semi_width) {
            result |= 0b01110000;
        }

        else if (cell_position.x < semi_width) {
            result |= 0b01010000;
        }
    }

    return result;
}
void GridEntry::get_partitions(uint8_t partition, const kinetra::Vec2 &position, const kinetra::Vec2 &origin,
                               const float cell_size, const GridEntry &grid_size, GridEntry output[4]) {
    GridEntry cell;

    if (!from_world(position, origin, cell_size, grid_size, cell))
        return;

    output[0] = cell;

    if (partition & 0b00000001) {
        if ((partition & 0b00000010) == 0b00000010) {
            if (const GridEntry t{cell.row, cell.column - 1}; in_bounds(t, grid_size))
                output[1] = t;
        }

        else {
            if (const GridEntry t{cell.row, cell.column + 1}; in_bounds(t, grid_size))
                output[1] = t;
        }
    }

    if (partition & 0b00000100) {
        if ((partition & 0b00001000) == 0b00001000) {
            if (const GridEntry t{cell.row - 1, cell.column}; in_bounds(t, grid_size))
                output[2] = t;
        }

        else {
            if (const GridEntry t{cell.row + 1, cell.column}; in_bounds(t, grid_size))
                output[2] = t;
        }
    }

    if (partition & 0b00010000) {
        if ((partition & 0b01100000) == 0b01100000) {
            if (const GridEntry t{cell.row - 1, cell.column + 1}; in_bounds(t, grid_size)) {
                output[3] = t;
            }
        }

        else if ((partition & 0b01000000) == 0b01000000) {
            if (const GridEntry t{cell.row - 1, cell.column - 1}; in_bounds(t, grid_size))
                output[3] = t;

        }

        else if ((partition & 0b00100000) == 0b00100000) {
            if (const GridEntry t{cell.row + 1, cell.column - 1}; in_bounds(t, grid_size))
                output[3] = t;

        }

        else {
            if (const GridEntry t{cell.row + 1, cell.column + 1}; in_bounds(t, grid_size))
                output[3] = t;
        }
    }
}

bool GridEntry::operator==(const GridEntry &other) const {
    return this->row == other.row && this->column == other.column;
}

GridEntry GridEntry::null_grid_entry = {-1, -1};
} // namespace dotio::math
