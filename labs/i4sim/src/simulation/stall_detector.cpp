#include "i4sim/simulation/detail/stall_detector.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <cmath>

namespace i4sim::detail {
using namespace kinetra;
namespace {
/// A larger excursion than the small adjustments of stationary steering.
constexpr float reversal_distance_mm = 2;
/// Require an angle greater than 120 degrees between successive excursions.
constexpr float reversal_cosine = -0.5f;
constexpr unsigned oscillation_reversals = 2;
constexpr float opposing_cosine = -0.8f;

bool eligible(const Mover &mover) {
    const auto &coord = mover.coordination;
    return mover.moving() && !mover.held && !mover.route.empty() && !coord.junction_waiting &&
           !coord.exit_waiting && !coord.corridor_waiting && !coord.corridor_retreating && !coord.passing;
}

Vec2 heading(const Mover &mover) {
    return normalized(mover.route.front() - mover.body.position);
}

bool blocks_leg(const Mover &mover, const Mover &other, const MotionSettings &motion) {
    const auto start = mover.body.position;
    const auto delta = mover.route.front() - start;
    if (dot(other.body.position - start, delta) <= 0)
        return false;
    // Diagnose the next leg within steering reach. A distant mover on a later
    // leg may have left by the time the participant reaches it.
    const float reach = length(mover.body.half_extents) * 2 + motion.speed * motion.horizon +
                        braking_distance(motion.speed, motion.force);
    const auto end = start + limited(delta, reach);
    return swept_box_overlaps(start, end, mover.body.half_extents + clearance, other.body);
}

StallDetails diagnose(const Fleet &fleet, const MotionSettings &motion, std::size_t index, std::uint64_t tick,
                      unsigned reversals) {
    const auto &mover = fleet.movers[index];
    const auto forward = heading(mover);
    std::optional<std::size_t> opposing;
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        if (i == index)
            continue;
        const auto &neighbor = fleet.movers[i];
        if (!blocks_leg(mover, neighbor, motion))
            continue;
        if (!neighbor.moving() || neighbor.held)
            return {tick, StallReason::stationary_blocker, i};
        if (opposing || !eligible(neighbor))
            continue;
        const auto opposing_heading = heading(neighbor);
        // Reciprocal avoidance can keep a pair moving sideways in place. Judge
        // speed along each route leg to distinguish that from making headway.
        if (std::abs(dot(mover.body.velocity, forward)) <= stop_speed &&
            std::abs(dot(neighbor.body.velocity, opposing_heading)) <= stop_speed &&
            dot(forward, opposing_heading) < opposing_cosine && blocks_leg(neighbor, mover, motion))
            opposing = i;
    }
    if (opposing)
        return {tick, StallReason::opposing_mover, opposing};
    return {tick, reversals >= oscillation_reversals ? StallReason::oscillating : StallReason::unknown};
}

void record(EventLog &log, EventKind kind, std::size_t mover, std::uint64_t tick, const StallDetails &stall) {
    if (log.recording())
        log.add({.tick = tick, .kind = kind, .movers = {mover, 0}, .stall = stall});
}
} // namespace

void StallDetector::Track::observe_position(Vec2 position) {
    const auto delta = position - anchor;
    if (dot(delta, delta) < reversal_distance_mm * reversal_distance_mm)
        return;
    // Compare successive excursions so the detected axis follows the mover's
    // motion even when it starts oscillating perpendicular to its initial drift.
    const auto excursion = normalized(delta);
    if (dot(direction, excursion) < reversal_cosine)
        ++reversals;
    direction = excursion;
    anchor = position;
}

void StallDetector::observe(const Fleet &fleet, const MotionSettings &motion, std::uint64_t tick,
                            EventLog &log) {
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        const auto &mover = fleet.movers[i];
        auto &track = tracks_[i];
        const auto no_progress = fleet.progress[i].no_progress;
        const bool watching = eligible(mover) && no_progress > 0;
        // Route preparation can reset the counter and then immediately count
        // the next tick, even when the destination is unchanged.
        if (!watching || track.goal != mover.destination || no_progress <= track.last_no_progress) {
            if (track.stall)
                record(log, EventKind::stall_ended, i, tick, *track.stall);
            track = {.goal = mover.destination, .anchor = mover.body.position};
        }
        track.last_no_progress = no_progress;
        if (!watching || track.stall)
            continue;
        track.observe_position(mover.body.position);
        if (++track.observations >= stall_ticks) {
            track.stall = diagnose(fleet, motion, i, tick, track.reversals);
            record(log, EventKind::stall_started, i, tick, *track.stall);
        }
    }
}
} // namespace i4sim::detail
