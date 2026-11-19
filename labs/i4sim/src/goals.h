#pragma once
#include "event_log.h"
#include "fleet.h"
#include "floor_plan.h"
#include "predictive_passing.h"
#include "simulation.h"
#include "traffic.h"

#include <cstdint>

namespace i4sim::detail {
/// Goal changes and goal choices for movers: the one place that gives a mover a new goal,
/// so everything that depended on the old goal ends with it.
///
/// A short-lived view of references that `SimulationCore` builds for each call; it stores
/// no state and must not outlive the call. Every `Mover &` argument must be an element of
/// `fleet.movers`.
struct Goals {
    const FloorPlan &floor;
    Fleet &fleet;
    Traffic &traffic;
    PredictivePassing &passing;
    EventLog &log;
    const MotionSettings &motion;
    /// The current tick, for retry delays and event ticks.
    std::uint64_t tick;

    /// Sends `mover` to `destination`: ends its corridor recovery and passing plans, plans
    /// a new route, resets its progress measurement and clears a parking staging goal. A
    /// manual goal stays manual. Without a route the mover becomes unreachable.
    void set(Mover &mover, Vec2 destination);

    /// A place where `mover` can wait near `destination`: its own parking place when that
    /// is reachable, far enough away and (with parking traffic) not on another mover's
    /// route; otherwise the reachable free cell center with the lowest distance-plus-
    /// heatmap score that keeps clear of other movers, their goals and processing cells.
    /// Falls back to the parking place when no cell qualifies.
    Vec2 holding_position(const Mover &mover, Vec2 destination) const;

    /// True when `mover`, standing at `position`, would block the route of another active
    /// mover that is not itself returning to park or staging.
    bool parking_conflict(const Mover &mover, Vec2 position) const;

    /// With parking traffic on: sends each mover whose return to its parking place would
    /// block another route to a staging place instead, and sends staged movers home once
    /// their parking place is free.
    void coordinate_parking();
};
} // namespace i4sim::detail
