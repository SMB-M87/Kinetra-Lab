#pragma once
#include "simulation.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace i4sim {
/// Thresholds that decide when a mover counts as waiting or stuck.
struct FlowLimits {
    /// Speed below which a mover with an active goal waits, in mm/tick.
    float waiting_speed{0.05f};
    /// Distance to the destination within which slow motion is the final approach and
    /// not waiting, in mm.
    float arrival_distance{10};
    /// Distance by which a mover must get closer to its destination to count as
    /// progress, in mm.
    float progress_distance{10};
    /// Observations without progress after which a mover with an active goal counts as
    /// stuck.
    std::uint64_t stuck_ticks{2000};
};

/// Measures traffic flow of one running simulation from its public state.
///
/// Call `observe` once after every `Simulation::tick`, always with the same simulation.
/// A mover has an *active goal* while it is `moving` and not `blocked`. The metrics only
/// read the simulation, so observing never changes a run.
class FlowMetrics {
  public:
    /// Throws `std::invalid_argument` when a distance or speed is not finite and
    /// positive, or `stuck_ticks` is zero.
    explicit FlowMetrics(FlowLimits limits = {});
    /// Adds the state after the latest tick. Throws `std::invalid_argument` when the
    /// simulation has a different number of movers than the first one observed.
    void observe(const Simulation &simulation);
    /// Mover-ticks with an active goal, a speed below `waiting_speed` and more than
    /// `arrival_distance` from the destination.
    std::uint64_t waiting_ticks() const { return waiting_ticks_; }
    /// Distinct movers that were stuck at least once: with an active goal, their
    /// straight-line distance to the destination did not drop `progress_distance` below
    /// its best value for `stuck_ticks` observations. This covers standing still as well
    /// as moving back and forth without getting closer. A detour that leads away from the
    /// destination for that long also counts. A mover within `arrival_distance` of its
    /// destination is in its final approach and never counts, even if it stays there.
    std::size_t stuck_movers() const;
    /// Movers that were stuck at the latest observation.
    std::size_t stuck_now() const;

  private:
    struct Track {
        kinetra::Vec2 destination;
        float best{};
        std::uint64_t since{};
        bool active{}, stuck{}, ever_stuck{};
    };
    FlowLimits limits_;
    std::vector<Track> tracks_;
    bool started_{};
    std::uint64_t observations_{}, waiting_ticks_{};
};
} // namespace i4sim
