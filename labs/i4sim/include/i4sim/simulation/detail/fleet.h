#pragma once
#include "i4sim/simulation/simulation.h"

#include <kinetra/simulation/math.h>

#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace i4sim::detail {
/// A temporary target that overrides a mover's route for one tick.
struct TrafficControl {
    /// Traffic zone that issued the control, or `passing_control` for a passing plan.
    std::size_t zone;
    /// Where the mover steers this tick, in millimeters.
    Vec2 target;
    /// Whether the mover should slow down to stop at `target`.
    bool arrival;
};
/// `TrafficControl::zone` of a control that a passing plan issued.
inline constexpr std::size_t passing_control = std::numeric_limits<std::size_t>::max();

/// Route-following counters of one mover.
struct RouteProgress {
    /// Ticks since the last replanning attempt.
    unsigned replan{};
    /// Ticks since the remaining route last shrank by a progress step.
    unsigned no_progress{};
    /// Shortest remaining route length seen since the route changed, in millimeters.
    float best_remaining{};
    /// Route recoveries since the last goal; lengthens the wait before the next one.
    unsigned recovery_attempts{};
};

/// The movers and the per-mover data the components share. Every vector is indexed by
/// mover. `controls` and `stops` are rebuilt by traffic coordination in each tick and are
/// empty before the first one.
struct Fleet {
    /// All movers, in blueprint order.
    std::vector<Mover> movers;
    /// Same size and order as `movers`.
    std::vector<RouteProgress> progress;
    /// Temporary targets of this tick, from corridor recovery and passing plans.
    std::vector<std::optional<TrafficControl>> controls;
    /// Stop lines of this tick, before junctions, corridors and exit pockets.
    std::vector<std::vector<kinetra::Segment>> stops;
    /// The largest half extents over all movers, per axis.
    Vec2 max_half{};

    /// Index of `mover`, which must be an element of `movers`.
    std::size_t index(const Mover &mover) const { return std::size_t(&mover - movers.data()); }
};
} // namespace i4sim::detail
