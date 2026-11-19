#pragma once
#include "blueprint.h"
#include "simulation.h"

#include <kinetra/math.h>
#include <kinetra/spatial_grid.h>
#include <kinetra/weighted_grid.h>

#include <cstddef>
#include <span>
#include <vector>

namespace i4sim::detail {
/// Where one traffic zone lies: a junction cell or a straight single-cell corridor.
struct ZoneGeometry {
    /// Center and half size of the zone, in millimeters.
    Vec2 center, half_extents;
    /// True for a corridor, false for a junction.
    bool corridor{};
    /// For a corridor: true when it runs along the y axis.
    bool vertical{};
};

/// The static topology of a factory floor, built once from a blueprint: the weighted route
/// grid with its blocked cells, the borders of the free space, and the traffic zones.
///
/// Only the heatmap weights change after construction. Junctions come first in `zones()`,
/// then corridors, so zone `j` is a junction exactly when `j < junction_count()`.
class FloorPlan {
  public:
    /// Blocks the blueprint's forbidden zones and producer footprints, then finds the
    /// junctions and corridors (see docs/factory-simulation-architecture.md).
    explicit FloorPlan(const Blueprint &blueprint);

    /// The route grid: blocked cells and heatmap weights.
    const kinetra::WeightedGrid &grid() const { return grid_; }
    /// Segments between free and blocked cells and along the outer edge.
    std::span<const kinetra::Segment> borders() const { return borders_; }
    /// All traffic zones: junctions first, then corridors.
    std::span<const ZoneGeometry> zones() const { return zones_; }
    /// Number of junctions, which are the first zones.
    std::size_t junction_count() const { return junction_count_; }
    /// Number of corridors, which follow the junctions.
    std::size_t corridor_count() const { return zones_.size() - junction_count_; }
    /// Spatial index over the zones, each grown by the clearance; ids are zone indices.
    const kinetra::SpatialGrid &zone_index() const { return zone_index_; }

    /// Size of one grid cell, in millimeters.
    Vec2 cell_size() const { return cell_size_; }
    /// Size of the whole floor, in millimeters.
    Vec2 dimension() const { return dimension_; }
    /// The blueprint's largest mover extent, in millimeters.
    float mover_max_extent() const { return mover_max_extent_; }
    /// Where a mover stands to be processed at a producer whose processing corner is
    /// `processing_position`.
    Vec2 processing_goal(Vec2 processing_position) const {
        return processing_position + Vec2{mover_max_extent_, mover_max_extent_} / 2;
    }
    /// True when grid cell `id` holds a producer's processing goal; holding places avoid
    /// those cells.
    bool processing_cell(int id) const { return processing_cells_[std::size_t(id)]; }

    /// Sets each cell's route cost from the movers whose footprint corners lie in it, or to
    /// zero when `enabled` is false.
    void update_heatmap(std::span<const Mover> movers, bool enabled);

  private:
    kinetra::WeightedGrid grid_;
    kinetra::SpatialGrid zone_index_;
    std::vector<kinetra::Segment> borders_;
    std::vector<ZoneGeometry> zones_;
    std::size_t junction_count_{};
    std::vector<bool> processing_cells_;
    Vec2 cell_size_, dimension_;
    float mover_max_extent_{};
    void find_junctions();
    void find_corridors();
};
} // namespace i4sim::detail
