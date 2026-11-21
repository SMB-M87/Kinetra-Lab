#pragma once
#include "i4sim/simulation/detail/event_log.h"
#include "i4sim/simulation/detail/fleet.h"
#include "i4sim/simulation/detail/floor_plan.h"
#include "i4sim/simulation/simulation.h"

#include <kinetra/simulation/math.h>
#include <kinetra/simulation/spatial_grid.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace i4sim::detail {
/// Predictive passing on the open floor: two movers that will meet head-on agree on two
/// side lanes, each drives to its entry target, and once both stand there they cross to
/// their exit targets. See docs/factory-simulation-architecture.md#predictive-passing.
///
/// A plan overrides its movers' routes with traffic controls; it never changes their
/// goals. In every tick it is checked against the movers around it: a participant waits
/// at its braking point while a mover with priority is predicted to enter its lane ahead,
/// and the plan ends as `obstructed` when a stationary mover stands on a remaining leg, a
/// mover drives toward the participant along that lane, or a wait does not end. Movers
/// behind a participant are ignored. Ending a plan replans both routes; after a plan that did not
/// complete, the movers join a new plan only after `passing_retry_ticks`. Plan ids start
/// at 1 and advance whether or not events are recorded, so recording cannot change them.
class PredictivePassing {
  public:
    /// True when `motion` turns passing on for a steering profile that supports it.
    static bool enabled(const MotionSettings &motion);

    /// Number of active plans.
    std::size_t pairs() const { return plans_.size(); }
    /// The target `mover` currently heads for in a plan: its braking point while it waits,
    /// otherwise its entry target before crossing and its exit target after.
    std::optional<Vec2> target(std::size_t mover) const;

    /// Ends every plan of `mover` with `PassingEnd::goal_changed`; called when it receives
    /// a goal. The routes of both participants are planned again under `motion`.
    void cancel(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::size_t mover,
                std::uint64_t tick, EventLog &log);

    /// Ends every plan with `PassingEnd::disabled`; called when `motion` turns passing off.
    /// The routes of all participants are planned again under `motion`.
    void disable(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                 EventLog &log);

    /// Advances and ends existing plans, forms new ones when passing is enabled, and sets
    /// the controls and `Coordination::passing` of every participant. Call after corridor
    /// recovery has set its controls, so movers under traffic control are left out.
    /// `index` holds the movers' bounds.
    void coordinate(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                    const kinetra::SpatialGrid &index, EventLog &log);

  private:
    /// One agreed passing maneuver between `movers[0]` and `movers[1]`.
    struct Plan {
        std::uint64_t id{};
        std::array<std::size_t, 2> movers;
        /// Per participant, the lane entry and exit targets.
        std::array<Vec2, 2> entry, exit;
        /// Swept areas of both participants, reserved against other plans.
        std::vector<kinetra::Aabb> space;
        /// Both participants reached their entry targets.
        bool crossing{};
        /// Least summed distance to the current targets, and the tick it was reached.
        float best_remaining{};
        std::uint64_t progress_at{};
        /// Per participant: waiting for a mover with priority to leave the lane ahead.
        std::array<bool, 2> waiting{};
        /// Per participant: tick its current wait began, and the last tick a mover with
        /// priority was predicted in its lane ahead.
        std::array<std::uint64_t, 2> waiting_since{}, intruded_at{};
        /// Per participant: where it stops while it waits, its braking point.
        std::array<Vec2, 2> hold{};
    };
    std::vector<Plan> plans_;
    /// Per mover: first tick at which it may join a new plan.
    std::vector<std::uint64_t> retry_;
    std::uint64_t next_plan_{1};

    void advance_plans(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                       const kinetra::SpatialGrid &index, EventLog &log);
    /// Rechecks the remaining legs of each participant of `plan` against the other movers:
    /// starts and ends waits, and returns `PassingEnd::obstructed` when a stationary mover
    /// stands on a remaining leg, a mover with priority drives toward the participant in
    /// its lane ahead, or a wait lasted `passing_timeout_ticks`.
    std::optional<PassingEnd> guard(Plan &plan, const Fleet &fleet, const MotionSettings &motion,
                                    std::uint64_t tick, const kinetra::SpatialGrid &index) const;
    /// Whether `plan` must yield to `other`: always when `other` is in no plan, otherwise
    /// only to a participant of an older plan.
    bool yields_to(const Plan &plan, std::size_t other) const;
    /// Updates the summed progress of `plan`; true when it went `passing_timeout_ticks`
    /// without progress.
    static bool stalled(Plan &plan, const Fleet &fleet, const MotionSettings &motion, std::uint64_t tick);
    void form_plans(const FloorPlan &floor, const Fleet &fleet, const MotionSettings &motion,
                    std::uint64_t tick, const kinetra::SpatialGrid &index, EventLog &log);
    bool try_form(std::size_t a, std::size_t b, const std::vector<Vec2> &directions, const FloorPlan &floor,
                  const Fleet &fleet, std::uint64_t tick, const kinetra::SpatialGrid &index, EventLog &log);
    bool lanes_clear(Plan &plan, const FloorPlan &floor, const Fleet &fleet,
                     const kinetra::SpatialGrid &index) const;
    void end(std::size_t index, PassingEnd reason, const FloorPlan &floor, Fleet &fleet,
             const MotionSettings &motion, std::uint64_t tick, EventLog &log);
    /// Plans both routes again and resets the participants' progress. `retry_later` makes
    /// them wait `passing_retry_ticks` before they may join a new plan.
    void restore_routes(std::array<std::size_t, 2> movers, const FloorPlan &floor, Fleet &fleet,
                        const MotionSettings &motion, std::uint64_t tick, bool retry_later);
    static void record(EventKind kind, const Plan &plan, std::uint64_t tick, EventLog &log,
                       std::optional<PassingEnd> end = {});
};
} // namespace i4sim::detail
