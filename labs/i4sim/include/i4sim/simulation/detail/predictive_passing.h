#pragma once
#include "i4sim/simulation/detail/event_log.h"
#include "i4sim/simulation/detail/fleet.h"
#include "i4sim/simulation/detail/floor_plan.h"
#include "i4sim/simulation/simulation.h"

#include <kinetra/simulation/math.h>
#include <kinetra/simulation/passing_coordinator.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace i4sim::detail {
/// Predictive passing on the open floor, run by Kinetra's `PassingCoordinator`: two movers
/// that will meet head-on agree on two side lanes, each drives to its entry target, and once
/// both stand there they cross to their exit targets. See
/// docs/factory-simulation-architecture.md#predictive-passing.
///
/// This adapter tells the coordinator what each mover is in the factory, checks lanes
/// against the floor and its traffic zones, logs the coordinator's events with the factory's
/// end reasons, and turns each participant's target into a traffic control. A plan never
/// changes its movers' goals. Ending a plan replans both routes; after a plan that failed,
/// the movers join a new plan only after `passing_retry_ticks`. Plan ids start at 1 and
/// advance whether or not events are recorded, so recording cannot change them.
class PredictivePassing {
  public:
    /// True when `motion` turns passing on for a steering profile that supports it.
    static bool enabled(const MotionSettings &motion);
    /// Number of active plans.
    std::size_t pairs() const { return coordinator_.size(); }
    /// The target `mover` currently heads for in a plan: its braking point while it waits,
    /// otherwise its entry target before crossing and its exit target after.
    std::optional<Vec2> target(std::size_t mover) const;
    /// The active plan for this mover index, if it participates in one.
    std::optional<Simulation::PassingDetails> details(std::size_t mover) const;
    /// Ends every plan of `mover` with `PassingEnd::goal_changed`; called when it receives
    /// a goal. The routes of both participants are planned again under `motion`, and both
    /// may join a new plan at once.
    void cancel(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::size_t mover,
                std::uint64_t tick, EventLog &log);
    /// Ends every plan with `PassingEnd::disabled`; called when `motion` turns passing off.
    /// The routes of all participants are planned again under `motion`.
    void disable(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                 EventLog &log);
    /// Advances and ends existing plans, forms new ones when passing is enabled, and sets
    /// the controls and `Coordination::passing` of every participant. Call after corridor
    /// recovery has set its controls, so movers under traffic control are left out.
    /// Movers with a temporary make-way goal retain their checked routes.
    void coordinate(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                    EventLog &log);

  private:
    kinetra::PassingCoordinator coordinator_;

    /// Logs `events` and replans the routes of the movers of every ended plan. `cancelled`
    /// is the factory's reason for a plan the coordinator reports as cancelled.
    static void apply(std::span<const kinetra::PassingEvent> events, PassingEnd cancelled,
                      const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                      EventLog &log);
};
} // namespace i4sim::detail
