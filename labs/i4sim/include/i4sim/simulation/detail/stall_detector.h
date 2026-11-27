#pragma once
#include "i4sim/simulation/detail/event_log.h"
#include "i4sim/simulation/detail/fleet.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace i4sim::detail {
/// Observes route stalls after each completed tick. Never writes to fleet or traffic state.
class StallDetector {
  public:
    /// Allocate observation state for the fixed number of movers in the fleet.
    explicit StallDetector(std::size_t movers) : tracks_(movers) {}

    /// Observes the same fleet, in the same order, once after each tick.
    void observe(const Fleet &fleet, const MotionSettings &motion, std::uint64_t tick, EventLog &log);
    /// Current episode's onset diagnosis. Throws std::out_of_range for an unknown mover.
    std::optional<StallDetails> details(std::size_t mover) const { return tracks_.at(mover).stall; }

  private:
    struct Track {
        Vec2 goal{};
        Vec2 anchor{};
        Vec2 direction{};
        unsigned observations{};
        unsigned last_no_progress{};
        unsigned reversals{};
        std::optional<StallDetails> stall{};

        void observe_position(Vec2 position);
    };
    std::vector<Track> tracks_;
};
} // namespace i4sim::detail
