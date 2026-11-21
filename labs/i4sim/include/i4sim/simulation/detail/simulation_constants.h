#pragma once
// Tuning values shared by the i4sim simulation components. Positions are in millimeters,
// speeds in millimeters per tick and durations in ticks (200 ticks per simulated second).
//
// Each value replaces a literal that was repeated in the code. The golden and pressure
// suites pin the exact results, so a changed value is a behavior change.
#include <kinetra/simulation/math.h>

#include <cstdint>

namespace i4sim::detail {
/// Clearance in millimeters added per axis around a footprint when checking routes, zones
/// and parking places; also the steering clearance between movers.
inline constexpr float clearance_mm = 2;
/// `clearance_mm` on both axes.
inline constexpr kinetra::Vec2 clearance{clearance_mm, clearance_mm};

/// Speed in millimeters per tick below which a mover counts as standing still.
inline constexpr float stop_speed = 0.05f;
/// Distance in millimeters below which two goal positions count as the same place.
inline constexpr float same_point_mm = 0.05f;
/// Distance in millimeters below which a mover counts as arrived at a target.
inline constexpr float arrival_distance_mm = 0.1f;
/// Distance in millimeters at which a route waypoint counts as reached, so the mover can
/// head for the next one when the straight line there is clear.
inline constexpr float waypoint_reach_mm = 3;

/// Ticks without route progress after which a mover counts as an obstacle for others'
/// route refinement, and after which its own route may be replanned.
inline constexpr unsigned obstruction_ticks = 100;
/// Ticks between two replanning attempts of a mover that makes no progress.
inline constexpr unsigned replan_ticks = 100;
/// Ticks without route progress after which a mover counts as stalled: corridor recovery
/// may start, the HUD shows STALLED, and route recovery makes its first attempt.
inline constexpr unsigned stall_ticks = 120;
/// Extra ticks route recovery waits per earlier attempt, so a mover that keeps failing
/// does not plan a Navigation route every tick.
inline constexpr unsigned recovery_backoff_ticks = 30;
/// Number of earlier attempts after which the route recovery wait stops growing.
inline constexpr unsigned recovery_backoff_limit = 10;
/// Ticks a corridor waits after a recovery ended before it tries a new one.
inline constexpr std::uint64_t corridor_retry_ticks = 120;
/// Ticks a mover waits after its passing plan ended without completing before it may join
/// a new one.
inline constexpr std::uint64_t passing_retry_ticks = 120;
/// Ticks a passing plan may go without its participants getting closer to their targets.
inline constexpr std::uint64_t passing_timeout_ticks = 240;
/// Ticks one producer interaction takes, as in the original factory.
inline constexpr unsigned processing_ticks = 1000;
/// Heatmap weights are refreshed on the first tick of every period of this many ticks.
inline constexpr std::uint64_t heatmap_period_ticks = 10;
/// Share of the remaining route length that counts as progress, so the slowing final
/// approach still counts as converging.
inline constexpr float progress_step_fraction = 0.05f;

/// Distance in millimeters a mover at `speed` mm/tick needs to stop when it brakes with
/// `force` mm/tick²: the `speed * speed / force` bound the coordination code reserves.
constexpr float braking_distance(float speed, float force) {
    return speed * speed / force;
}
} // namespace i4sim::detail
