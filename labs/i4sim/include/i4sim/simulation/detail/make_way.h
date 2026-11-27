#pragma once
#include "i4sim/simulation/detail/goals.h"
#include "i4sim/simulation/detail/shuttle.h"
#include "i4sim/simulation/detail/stall_detector.h"

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

  private:
    enum class Phase { outbound, waiting, returning };
    struct Trip {
        std::size_t beneficiary;
        Vec2 goal;
        std::uint64_t deadline;
        Phase phase{Phase::outbound};
        std::optional<MakeWayEnd> outcome{};
    };
    /// Indexed by the yielding mover. A vacant entry allows it to be considered again.
    std::vector<std::optional<Trip>> trips_;

    struct Attempt {
        std::uint64_t episode{};
        std::uint64_t retry_at{};
        unsigned searches{};
    };
    /// Bounded geometry searches for the active stall episode, by beneficiary.
    std::vector<Attempt> attempts_;
    void advance_trip(Goals &goals, std::size_t mover);
    void try_make_way(Goals &goals, std::span<const Product> products, const Shuttle &shuttle,
                      const StallDetector &stalls, std::size_t beneficiary);
};
} // namespace i4sim::detail
