#pragma once
#include "i4sim/simulation/simulation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace i4sim::detail {
/// One leg the shuttle wants a mover to drive next.
struct ShuttleLeg {
    /// Index of the mover in the fleet.
    std::size_t mover;
    /// The end the mover heads for: a mover center, in millimeters.
    Vec2 destination;
};

/// Movers that drive back and forth between two ends and turn together; the policy behind
/// `Simulation::shuttle`.
///
/// Decides only: `update` returns the legs to send, and the caller gives the goals and
/// reports a rejected leg with `leave`. Holds mover indices and positions, no references,
/// so copying a simulation copies it safely.
class Shuttle {
  public:
    /// Adds `mover`, now driving from `from` to `to`, or replaces its ends when it is already
    /// a member.
    void join(std::size_t mover, Vec2 from, Vec2 to);
    /// Removes `mover`; does nothing when it is not a member.
    void leave(std::size_t mover);
    /// True while `mover` is a member.
    bool contains(std::size_t mover) const;
    /// Advances the shuttle after tick `tick`; call it once at the end of every tick.
    ///
    /// First removes members that left: unreachable, carrying a product, or following a goal
    /// that is not manual. Once every remaining member is idle and not held, it waits
    /// `shuttle_pause_ticks`; then it returns one leg per member, to the end the member did
    /// not head for last, in join order. Any member that moves or is held during the pause
    /// restarts the wait. Returns an empty list otherwise. `movers` must hold every member.
    std::vector<ShuttleLeg> update(const std::vector<Mover> &movers, std::uint64_t tick);

  private:
    struct Member {
        std::size_t mover;
        /// The two ends, in millimeters; `heading` indexes the one of the current leg.
        std::array<Vec2, 2> ends;
        std::size_t heading;
    };
    std::vector<Member> members_;
    /// Tick at which every member was last seen arrived; empty while one is under way.
    std::optional<std::uint64_t> arrived_;
};
} // namespace i4sim::detail
