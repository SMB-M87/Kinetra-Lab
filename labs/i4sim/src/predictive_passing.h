#pragma once
#include "event_log.h"
#include "fleet.h"
#include "floor_plan.h"
#include "simulation.h"

#include <kinetra/math.h>
#include <kinetra/spatial_grid.h>

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
/// goals. Ending a plan replans both routes and lets the movers join a new plan only after
/// `passing_retry_ticks`. Plan ids start at 1 and advance whether or not events are
/// recorded, so recording cannot change them.
class PredictivePassing {
  public:
    /// True when `motion` turns passing on for a steering profile that supports it.
    static bool enabled(const MotionSettings &motion);

    /// Number of active plans.
    std::size_t pairs() const { return plans_.size(); }
    /// The target `mover` currently heads for in a plan: its entry target before crossing,
    /// its exit target after.
    std::optional<Vec2> target(std::size_t mover) const;

    /// Ends every plan of `mover` with `PassingEnd::goal_changed`; called when it receives
    /// a goal.
    void cancel(const FloorPlan &floor, Fleet &fleet, std::size_t mover, std::uint64_t tick, EventLog &log);

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
    };
    std::vector<Plan> plans_;
    /// Per mover: first tick at which it may join a new plan.
    std::vector<std::uint64_t> retry_;
    std::uint64_t next_plan_{1};

    void advance_plans(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                       EventLog &log);
    void form_plans(const FloorPlan &floor, const Fleet &fleet, const MotionSettings &motion,
                    std::uint64_t tick, const kinetra::SpatialGrid &index, EventLog &log);
    bool try_form(std::size_t a, std::size_t b, const std::vector<Vec2> &directions, const FloorPlan &floor,
                  const Fleet &fleet, std::uint64_t tick, const kinetra::SpatialGrid &index, EventLog &log);
    bool lanes_clear(Plan &plan, const FloorPlan &floor, const Fleet &fleet,
                     const kinetra::SpatialGrid &index) const;
    void end(std::size_t index, PassingEnd reason, const FloorPlan &floor, Fleet &fleet, std::uint64_t tick,
             EventLog &log);
    void restore_routes(std::array<std::size_t, 2> movers, const FloorPlan &floor, Fleet &fleet,
                        std::uint64_t tick);
    static void record(EventKind kind, const Plan &plan, std::uint64_t tick, EventLog &log,
                       std::optional<PassingEnd> end = {});
};
} // namespace i4sim::detail
