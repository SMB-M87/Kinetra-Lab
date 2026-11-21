#include "dotio/navigation/grid.h"
#include "dotio/entities/physics_adapter.h"
#include "dotio/math/random.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace dotio {
Grid::Grid() : navigation_({columns * cell_size, rows * cell_size}, {cell_size, cell_size}) {
}

void Grid::set_cell(int row, int column, int value) {
    cells_[row][column] = value;
    navigation_.block(row * columns + column, value != 0);
}

void Grid::fill(int value) {
    for (int row = 0; row < rows; ++row)
        for (int column = 0; column < columns; ++column)
            set_cell(row, column, value);
}

kinetra::CollisionGrid Grid::collision_view() const {
    return {{&cells_[0][0], std::size_t(rows * columns)}, rows, columns, cell_size, {origin.x, origin.y}};
}

// Seeded depth-first mazes reproduce the narrow, irregular passages of the original
// hand-authored map. Shared edge ports join independently generated 32-cell districts.
int Grid::terrain_cell(std::uint32_t seed, int row, int column) const {
    constexpr int size = 32;
    const int y = (row % size + size) % size, x = (column % size + size) % size;
    const int dy = (row - y) / size, dx = (column - x) / size;
    using Cells = std::array<int, size * size>;
    auto &cache = terrain_cache_;
    const auto key = std::tuple{seed, dy, dx};
    if (const auto found = cache.find(key); found != cache.end())
        return found->second[y * size + x];
    if (cache.size() > 128)
        cache.clear();
    Cells cells;
    cells.fill(1);
    std::seed_seq sequence{seed, std::uint32_t(dy), std::uint32_t(dx)};
    std::mt19937 random(sequence);
    struct Node {
        int x, y;
    };
    std::array<Node, 64> nodes;
    for (int i = 0; i < 64; ++i)
        nodes[i] = {i % 8 * 4 + 1 + int(random() % 3), i / 8 * 4 + 1 + int(random() % 3)};
    const auto carve = [&](int px, int py) { cells[py * size + px] = 0; };
    // Jittered junctions and independently chosen bends avoid a visible node lattice.
    const auto connect = [&](Node from, Node to) {
        const bool horizontal_first = random() % 2;
        const auto horizontal = [&]() {
            while (from.x != to.x) {
                carve(from.x, from.y);
                from.x += from.x < to.x ? 1 : -1;
            }
        };
        const auto vertical = [&]() {
            while (from.y != to.y) {
                carve(from.x, from.y);
                from.y += from.y < to.y ? 1 : -1;
            }
        };
        if (horizontal_first) {
            horizontal();
            vertical();
        } else {
            vertical();
            horizontal();
        }
        carve(to.x, to.y);
    };
    std::array<bool, 64> visited{};
    std::vector<int> stack{0};
    visited[0] = true;
    carve(nodes[0].x, nodes[0].y);
    while (!stack.empty()) {
        const int current = stack.back(), cx = current % 8, cy = current / 8;
        std::array<int, 4> directions{0, 1, 2, 3};
        std::shuffle(directions.begin(), directions.end(), random);
        bool advanced = false;
        for (int direction : directions) {
            const int nx = cx + (direction == 0) - (direction == 1);
            const int ny = cy + (direction == 2) - (direction == 3);
            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8 || visited[ny * 8 + nx])
                continue;
            connect(nodes[current], nodes[ny * 8 + nx]);
            visited[ny * 8 + nx] = true;
            stack.push_back(ny * 8 + nx);
            advanced = true;
            break;
        }
        if (!advanced)
            stack.pop_back();
    }
    // Braid the spanning tree so most junctions offer alternative routes.
    for (int i = 0; i < 64; ++i) {
        if (i % 8 < 7 && random() % 100 < 65)
            connect(nodes[i], nodes[i + 1]);
        if (i / 8 < 7 && random() % 100 < 65)
            connect(nodes[i], nodes[i + 8]);
        if (random() % 4 == 0) {
            const int width = 1 + int(random() % 2), height = 1 + int(random() % 2);
            for (int ry = std::max(1, nodes[i].y - height); ry <= std::min(30, nodes[i].y + height); ++ry)
                for (int rx = std::max(1, nodes[i].x - width); rx <= std::min(30, nodes[i].x + width); ++rx)
                    carve(rx, ry);
        }
    }
    // Both sides seed an edge identically; multiple varied ports join adjacent districts.
    for (int edge = 0; edge < 4; ++edge) {
        const bool horizontal = edge < 2;
        std::seed_seq edge_seed{seed, std::uint32_t(dy + (edge == 1)), std::uint32_t(dx + (edge == 3)),
                                std::uint32_t(horizontal)};
        std::mt19937 edge_random(edge_seed);
        for (int section = 0; section < 4; ++section) {
            const int port = section * 8 + 1 + int(edge_random() % 6);
            const int node =
                horizontal ? (edge == 0 ? 0 : 56) + port / 4 : (port / 4) * 8 + (edge == 2 ? 0 : 7);
            connect(nodes[node],
                    horizontal ? Node{port, edge == 0 ? 0 : 31} : Node{edge == 2 ? 0 : 31, port});
        }
    }
    // Connect the launch clearing to the generated network.
    if (dy == 2 && dx == 2) {
        connect(nodes[36], {18, 18});
        for (int ry = 15; ry <= 18; ++ry)
            for (int rx = 15; rx <= 18; ++rx)
                carve(rx, ry);
    }
    const int result = cells[y * size + x];
    cache.emplace(key, cells);
    return result;
}

void Grid::generate(std::uint32_t seed, int offset_row, int offset_column) {
    for (int row = 0; row < rows; ++row)
        for (int column = 0; column < columns; ++column)
            set_cell(row, column, terrain_cell(seed, row + offset_row, column + offset_column));
}

void Grid::handle_grid_collision(dotio::PhysicsObject &object) const {
    auto body = dotio::physics_body(object);
    kinetra::collide_grid(body, collision_view());
    dotio::apply_body(object, body);
}

bool Grid::segment_is_intersecing_circle(const dotio::math::Segment &segment,
                                         const dotio::math::Point2 center, const float radius) {
    dotio::math::Point2 hit;
    return segement_circle_intersection(segment, center, radius, hit) & 1;
}

uint8_t Grid::segement_circle_intersection(const dotio::math::Segment &segment,
                                           const dotio::math::Point2 center, const float radius,
                                           dotio::math::Point2 &intersection) {
    const auto start = dotio::physics_vector(segment.start), end = dotio::physics_vector(segment.end);
    const auto c = dotio::physics_vector(center);
    const uint8_t flags =
        (kinetra::length(start - c) <= radius ? 2 : 0) | (kinetra::length(end - c) <= radius ? 4 : 0);
    if (const auto fraction = kinetra::segment_circle(start, end, c, radius)) {
        intersection = dotio::legacy_vector(start + (end - start) * *fraction);
        return flags | 1;
    }
    return flags;
}

bool Grid::is_intersecting_any_cell(dotio::math::Segment segment, dotio::math::Point2 &intersection) const {
    if (const auto fraction = kinetra::segment_grid(dotio::physics_vector(segment.start),
                                                    dotio::physics_vector(segment.end), collision_view())) {
        intersection = segment.start + (segment.end - segment.start) * *fraction;
        return true;
    }
    return false;
}

dotio::math::GridEntry Grid::generate_grid_entry(dotio::math::GridEntry around, std::mt19937 &random) const {
    const int center_row = std::clamp(around.row, 0, rows - 1);
    const int center_col = std::clamp(around.column, 0, columns - 1);
    std::vector<dotio::math::GridEntry> candidates;
    // Enumerate once: unlike rejection sampling, sealed neighborhoods cannot hang.
    for (int row = std::max(0, center_row - 5); row <= std::min(rows - 1, center_row + 5); ++row)
        for (int col = std::max(0, center_col - 5); col <= std::min(columns - 1, center_col + 5); ++col)
            if (!cells_[row][col])
                candidates.push_back({row, col});
    if (candidates.empty())
        throw std::runtime_error("No free Dotio wander cell in the local neighborhood");
    return candidates[std::size_t(dotio::math::index(random, int(candidates.size())))];
}

dotio::math::Position Grid::generate_position(std::mt19937 &random) const {
    // Rows and columns from 3 to 152, both included, as the original game chose them.
    constexpr int margin = 3;
    constexpr int choices = rows - 2 * margin + 1;
    int row, column;
    do {
        row = margin + dotio::math::index(random, choices);
        column = margin + dotio::math::index(random, choices);
    } while (cells_[row][column] != 0);
    float pos_x = column * dotio::Grid::cell_size + (dotio::Grid::cell_size * 0.5f);
    float pos_y = row * dotio::Grid::cell_size + (dotio::Grid::cell_size * 0.5f);
    return {pos_x, pos_y};
}
} // namespace dotio