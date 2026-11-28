#pragma once
#include "i4sim/simulation/detail/goals.h"
#include "i4sim/simulation/detail/shuttle.h"
#include "i4sim/simulation/detail/stall_detector.h"

#include <kinetra/simulation/make_way.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace i4sim::detail {
/// Advances temporary parking trips based on episodes from StallDetector.
class MakeWay {
  public:
    /// Advances existing trips, then considers new requests if enabled.
    void update(Goals &goals, std::span<const Product> products, const Shuttle &shuttle,
                const StallDetector &stalls);

    /// True if the route avoids currently enabled zones and their reserved exits,
    /// or withdraws outward from an area the mover already overlaps.
    static bool traffic_clear(const Goals &goals, const Mover &mover, std::span<const Vec2> route);
    /// True if dropping a waypoint keeps the new first leg clear of live footprints
    /// and the remaining route clear of traffic reservations. The caller checks static
    /// floor clearance of the new leg. `remaining` must contain at least one point.
    static bool shortcut_clear(const Goals &goals, const Mover &mover, std::span<const Vec2> remaining);

  private:
    kinetra::MakeWay lifecycle_;
    /// Per mover whose trip ended while it still yields, the tick since which it stands still on
    /// its own: neither moving home nor held.
    std::vector<std::optional<std::uint64_t>> ended_;
    /// Gives a yielding mover back to the factory when it has stood still on its own for
    /// `stranded_ticks` after its trip ended: its way back stays blocked, often by the mover it
    /// made way for, which in turn waits for it. Without a product it heads for its parking place.
    void release_stranded(Goals &goals);
};
} // namespace i4sim::detail
