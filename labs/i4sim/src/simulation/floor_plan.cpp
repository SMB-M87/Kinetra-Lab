#include "i4sim/simulation/detail/floor_plan.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <algorithm>

namespace i4sim::detail {
using namespace kinetra;

FloorPlan::FloorPlan(const Blueprint &blueprint)
    : grid_(blueprint.dimension, blueprint.cell_size),
      zone_index_(std::max(blueprint.cell_size.x, blueprint.cell_size.y)), cell_size_(blueprint.cell_size),
      dimension_(blueprint.dimension), mover_max_extent_(blueprint.mover_max_extent) {
    auto block_area = [&](Vec2 p, Vec2 size) {
        for (float y = p.y; y < p.y + size.y; y += cell_size_.y)
            for (float x = p.x; x < p.x + size.x; x += cell_size_.x) {
                const int id = grid_.cell(Vec2{x, y} + cell_size_ / 2);
                if (id >= 0)
                    grid_.block(id);
            }
    };
    for (const auto &z : blueprint.zones)
        block_area(z.position, z.dimension);
    const float radius = std::max(cell_size_.x, cell_size_.y) / 2;
    for (const auto &definition : blueprint.producers)
        // A producer already in a forbidden cell does not add another footprint.
        if (!grid_.blocked(grid_.cell(definition.position)))
            block_area(definition.position - Vec2{radius, radius}, {radius * 2, radius * 2});
    borders_ = grid_.borders();
    find_junctions();
    junction_count_ = zones_.size();
    find_corridors();
    std::vector<Aabb> boxes;
    for (const auto &zone : zones_) {
        const auto extent = zone.half_extents + clearance;
        boxes.push_back({zone.center - extent, zone.center + extent});
    }
    zone_index_.rebuild(boxes);
    processing_cells_.assign(std::size_t(grid_.width() * grid_.height()), false);
    for (const auto &definition : blueprint.producers)
        if (const int id = grid_.cell(processing_goal(definition.processing_position)); id >= 0)
            processing_cells_[std::size_t(id)] = true;
}

void FloorPlan::find_junctions() {
    auto open = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < grid_.width() && y < grid_.height() &&
               !grid_.blocked(y * grid_.width() + x);
    };
    auto degree = [&](int x, int y) {
        return int(open(x - 1, y)) + int(open(x + 1, y)) + int(open(x, y - 1)) + int(open(x, y + 1));
    };
    // Only isolated T/+ junctions with one-cell corridor arms. Adjacent junctions
    // and open factory floors need a richer zone/exit reservation policy.
    for (int y = 0; y < grid_.height(); ++y)
        for (int x = 0; x < grid_.width(); ++x) {
            if (!open(x, y) || degree(x, y) < 3)
                continue;
            bool corridors = true;
            for (auto delta : {Vec2{-1, 0}, Vec2{1, 0}, Vec2{0, -1}, Vec2{0, 1}}) {
                const int nx = x + int(delta.x), ny = y + int(delta.y);
                if (open(nx, ny) && degree(nx, ny) != 2)
                    corridors = false;
            }
            if (corridors)
                zones_.push_back({grid_.center(y * grid_.width() + x), cell_size_ / 2, false, false});
        }
}

void FloorPlan::find_corridors() {
    auto open = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < grid_.width() && y < grid_.height() &&
               !grid_.blocked(y * grid_.width() + x);
    };
    // Straight one-cell corridors are exclusive zones only when both ends open
    // into a two-cell-deep, three-cell-wide room. Dead ends and bends are excluded.
    for (bool vertical : {false, true}) {
        const int along_limit = vertical ? grid_.height() : grid_.width();
        const int across_limit = vertical ? grid_.width() : grid_.height();
        auto free = [&](int along, int across) {
            return vertical ? open(across, along) : open(along, across);
        };
        auto narrow = [&](int along, int across) {
            return free(along, across) && free(along - 1, across) && free(along + 1, across) &&
                   !free(along, across - 1) && !free(along, across + 1);
        };
        auto room = [&](int mouth, int direction, int across) {
            for (int depth = 0; depth < 2; ++depth)
                for (int side = -1; side <= 1; ++side)
                    if (!free(mouth + depth * direction, across + side))
                        return false;
            return true;
        };
        for (int across = 0; across < across_limit; ++across)
            for (int along = 0; along < along_limit; ++along) {
                if (!narrow(along, across) || narrow(along - 1, across))
                    continue;
                const int first = along;
                while (along + 1 < along_limit && narrow(along + 1, across))
                    ++along;
                const int last = along;
                // Bound zone indexing work for unusually large custom layouts.
                if (last - first + 1 > 256 || !room(first - 1, -1, across) || !room(last + 1, 1, across))
                    continue;
                const Vec2 cell_start =
                    vertical ? Vec2{float(across), float(first)} : Vec2{float(first), float(across)};
                const Vec2 cells =
                    vertical ? Vec2{1, float(last - first + 1)} : Vec2{float(last - first + 1), 1};
                const Vec2 size{cells.x * cell_size_.x, cells.y * cell_size_.y};
                const Vec2 low{cell_start.x * cell_size_.x, cell_start.y * cell_size_.y};
                zones_.push_back({low + size / 2, size / 2, true, vertical});
            }
    }
}

void FloorPlan::update_heatmap(std::span<const Mover> movers, bool enabled) {
    std::vector<float> weights(std::size_t(grid_.width() * grid_.height()));
    for (const auto &m : movers) {
        const Vec2 size = m.body.half_extents * 2;
        const float x = cell_size_.x / size.x, y = cell_size_.y / size.y;
        const float delta = (x < 2 && y < 2 ? 16.f : x < 2 || y < 2 ? 8.f : 4.f) / 4;
        for (Vec2 corner : {m.body.position - m.body.half_extents, m.body.position + m.body.half_extents,
                            m.body.position + Vec2{-m.body.half_extents.x, m.body.half_extents.y},
                            m.body.position + Vec2{m.body.half_extents.x, -m.body.half_extents.y}}) {
            const int id = grid_.cell(corner);
            if (id >= 0)
                weights[std::size_t(id)] += delta;
        }
    }
    if (!enabled)
        std::fill(weights.begin(), weights.end(), 0.f);
    grid_.weights(weights);
}
} // namespace i4sim::detail
