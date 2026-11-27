#pragma once
#include "i4sim/simulation/detail/event_log.h"
#include "i4sim/simulation/detail/fleet.h"

#include <kinetra/simulation/stall_detector.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace i4sim::detail {
/// The factory's tuning of stall observation, in millimeters and ticks; RIGHT OF WAY uses it
/// too, so a push means the same to both.
kinetra::StallObservation stall_observation(const MotionSettings &motion);

/// Observes route stalls after each completed tick with Kinetra's `StallDetector`: it tells
/// the detector what each mover is in the factory and how long it went without route
/// progress, and records the episodes in the event log. Never writes to fleet or traffic
/// state.
class StallDetector {
  public:
    /// Observes a fleet of `movers` movers.
    explicit StallDetector(std::size_t movers) : movers_(movers) {}
    /// Observes the same fleet, in the same order, once after each tick.
    void observe(const Fleet &fleet, const MotionSettings &motion, std::uint64_t tick, EventLog &log);
    /// Current episode's onset diagnosis. Throws std::out_of_range for an unknown mover.
    std::optional<StallDetails> details(std::size_t mover) const;

  private:
    std::size_t movers_;
    kinetra::StallDetector detector_;
};
} // namespace i4sim::detail
