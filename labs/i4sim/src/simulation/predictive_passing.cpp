#include "i4sim/simulation/detail/predictive_passing.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/routing.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <kinetra/simulation/passing.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace i4sim::detail {
using namespace kinetra;
namespace {
/// Gap in millimeters kept between two passing movers, and between a mover's exit target
/// and its goal; also the margin of the predicted-contact test.
constexpr float passing_gap_mm = 8;
/// How far past the midpoint, in millimeters beyond both bodies, the exit targets lie.
constexpr float exit_offset_mm = 16;
/// Extra longitudinal room in millimeters a plan needs, so both lane changes fit.
constexpr float formation_room_mm = 32;
/// Share of the maximum speed per tick that counts as progress toward the targets.
constexpr float progress_speed_fraction = 0.05f;
/// Ticks the lane ahead of a waiting participant must stay clear before it drives on, so
/// a mover passing close by does not make it start and stop in turn.
constexpr std::uint64_t passing_resume_ticks = 20;
/// Ticks between two predicted positions of another mover in the lane check; at the
/// default 2 mm/tick a mover moves 8 mm, far less than any footprint.
constexpr int prediction_step_ticks = 4;

PassingApproach passing_approach(const Mover &mover) {
    return {mover.body.position, mover.body.half_extents,
            mover.route.empty() ? Vec2{} : normalized(mover.route.front() - mover.body.position),
            mover.destination};
}
} // namespace

bool PredictivePassing::enabled(const MotionSettings &motion) {
    return motion.predictive_passing && supports_predictive_passing(motion.profile);
}

std::optional<Vec2> PredictivePassing::target(std::size_t mover) const {
    if (const auto plan = details(mover))
        return plan->target;
    return std::nullopt;
}

std::optional<Simulation::PassingDetails> PredictivePassing::details(std::size_t mover) const {
    for (const auto &plan : plans_)
        for (std::size_t side = 0; side < 2; ++side)
            if (plan.movers[side] == mover)
                return Simulation::PassingDetails{
                    plan.id, plan.movers[1 - side], plan.crossing, plan.waiting[side],
                    plan.waiting[side] ? plan.hold[side]
                                       : (plan.crossing ? plan.exit[side] : plan.entry[side])};
    return std::nullopt;
}

void PredictivePassing::restore_routes(std::array<std::size_t, 2> ids, const FloorPlan &floor, Fleet &fleet,
                                       const MotionSettings &motion, std::uint64_t tick, bool retry_later) {
    for (auto id : ids) {
        auto &m = fleet.movers[id];
        m.coordination.passing = false;
        m.route = plan_route(floor, fleet, motion, id, m.destination);
        m.lifecycle = m.route.empty() ? MoverLifecycle::unreachable : MoverLifecycle::moving;
        auto &tracking = fleet.progress[id];
        tracking.no_progress = tracking.replan = 0;
        tracking.best_remaining = route_remaining(m.body.position, m.route);
        if (id < fleet.controls.size())
            fleet.controls[id].reset();
        // After a failed plan the movers wait before they try again, so they do not thrash.
        // After a completed one they may pass the next oncoming mover at once: waiting only
        // lets it come too close for a plan.
        if (id < retry_.size())
            retry_[id] = retry_later ? tick + passing_retry_ticks : tick;
    }
}

void PredictivePassing::record(EventKind kind, const Plan &plan, std::uint64_t tick, EventLog &log,
                               std::optional<PassingEnd> end) {
    if (!log.recording())
        return;
    SimulationEvent event{.tick = tick, .kind = kind, .plan = plan.id, .movers = plan.movers};
    if (kind == EventKind::passing_formed) {
        event.entry = plan.entry;
        event.exit = plan.exit;
    }
    event.end = end;
    log.add(event);
}

void PredictivePassing::end(std::size_t index, PassingEnd reason, const FloorPlan &floor, Fleet &fleet,
                            const MotionSettings &motion, std::uint64_t tick, EventLog &log) {
    record(EventKind::passing_ended, plans_[index], tick, log, reason);
    const auto ids = plans_[index].movers;
    plans_.erase(plans_.begin() + std::ptrdiff_t(index));
    restore_routes(ids, floor, fleet, motion, tick, reason != PassingEnd::completed);
}

void PredictivePassing::cancel(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                               std::size_t mover, std::uint64_t tick, EventLog &log) {
    for (std::size_t i = 0; i < plans_.size();) {
        const auto ids = plans_[i].movers;
        if (mover != ids[0] && mover != ids[1]) {
            ++i;
            continue;
        }
        end(i, PassingEnd::goal_changed, floor, fleet, motion, tick, log);
    }
}

void PredictivePassing::disable(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                                std::uint64_t tick, EventLog &log) {
    while (!plans_.empty())
        end(0, PassingEnd::disabled, floor, fleet, motion, tick, log);
}

void PredictivePassing::coordinate(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                                   std::uint64_t tick, const SpatialGrid &index, EventLog &log) {
    retry_.resize(fleet.movers.size());
    advance_plans(floor, fleet, motion, tick, index, log);
    if (!enabled(motion))
        return;
    form_plans(floor, fleet, motion, tick, index, log);
    for (const auto &plan : plans_)
        for (std::size_t side = 0; side < 2; ++side) {
            const auto id = plan.movers[side];
            fleet.movers[id].coordination.passing = true;
            const auto target = plan.waiting[side] ? plan.hold[side]
                                : plan.crossing    ? plan.exit[side]
                                                   : plan.entry[side];
            fleet.controls[id] = TrafficControl{passing_control, target, plan.waiting[side]};
        }
}

void PredictivePassing::advance_plans(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                                      std::uint64_t tick, const SpatialGrid &index, EventLog &log) {
    for (std::size_t i = 0; i < plans_.size();) {
        auto &plan = plans_[i];
        // The first reason found ends the plan; the event log reports it.
        std::optional<PassingEnd> end_reason;
        if (!enabled(motion))
            end_reason = PassingEnd::disabled;
        bool arrived = true;
        for (std::size_t side = 0; side < 2; ++side) {
            const auto id = plan.movers[side];
            const auto &m = fleet.movers[id];
            const auto target = plan.crossing ? plan.exit[side] : plan.entry[side];
            arrived = arrived && distance(m.body.position, target) < arrival_distance_mm &&
                      length(m.body.velocity) < stop_speed;
            if (end_reason)
                continue;
            if (!m.moving())
                end_reason = PassingEnd::participant_stopped;
            else if (m.held)
                end_reason = PassingEnd::participant_held;
            else if (fleet.controls[id].has_value())
                end_reason = PassingEnd::traffic_control;
        }
        if (!end_reason)
            end_reason = guard(plan, fleet, motion, tick, index);
        if (stalled(plan, fleet, motion, tick) && !end_reason)
            end_reason = PassingEnd::no_progress;
        if (!end_reason && arrived && plan.crossing)
            end_reason = PassingEnd::completed;
        if (end_reason) {
            end(i, *end_reason, floor, fleet, motion, tick, log);
            continue;
        }
        if (arrived) {
            plan.crossing = true;
            plan.best_remaining = std::numeric_limits<float>::infinity();
            plan.progress_at = tick;
            record(EventKind::passing_crossing, plan, tick, log);
        }
        ++i;
    }
}

bool PredictivePassing::stalled(Plan &plan, const Fleet &fleet, const MotionSettings &motion,
                                std::uint64_t tick) {
    float remaining = 0;
    for (std::size_t side = 0; side < 2; ++side)
        remaining += distance(fleet.movers[plan.movers[side]].body.position,
                              plan.crossing ? plan.exit[side] : plan.entry[side]);
    if (remaining + std::max(0.0001f, motion.speed * progress_speed_fraction) < plan.best_remaining) {
        plan.best_remaining = remaining;
        plan.progress_at = tick;
    }
    return tick - plan.progress_at >= passing_timeout_ticks;
}

bool PredictivePassing::yields_to(const Plan &plan, std::size_t other) const {
    for (const auto &candidate : plans_)
        if (candidate.movers[0] == other || candidate.movers[1] == other)
            return candidate.id < plan.id;
    return true;
}

std::optional<PassingEnd> PredictivePassing::guard(Plan &plan, const Fleet &fleet,
                                                   const MotionSettings &motion, std::uint64_t tick,
                                                   const SpatialGrid &index) const {
    const int horizon = int(std::ceil(motion.horizon));
    const PassingPrediction prediction{motion.speed, motion.force, horizon, prediction_step_ticks};
    const auto phase = plan.crossing ? PassingPhase::traverse : PassingPhase::approach;
    const float margin = length(fleet.max_half) + motion.speed * float(horizon);
    std::optional<PassingEnd> result;
    for (std::size_t side = 0; side < 2; ++side) {
        const auto &m = fleet.movers[plan.movers[side]];
        const PassingLaneGuard guard(m.body, {plan.entry[side], plan.exit[side]}, phase, clearance,
                                     prediction);
        bool intruded = false;
        for (auto other : index.query(guard.query_bounds(margin))) {
            if (other == plan.movers[0] || other == plan.movers[1])
                continue;
            const auto &neighbor = fleet.movers[other];
            const bool stationary = !neighbor.moving() || neighbor.held;
            // A stationary mover is an obstruction regardless of passing priority.
            if (!stationary && !yields_to(plan, other))
                continue;
            const auto obstacle = guard.inspect(neighbor.body, stationary);
            if (obstacle == PassingObstruction::blocking)
                result = PassingEnd::obstructed;
            if (obstacle == PassingObstruction::crossing)
                intruded = true;
        }
        if (intruded) {
            plan.intruded_at[side] = tick;
            if (!plan.waiting[side]) {
                plan.waiting[side] = true;
                plan.waiting_since[side] = tick;
                // Within the braking distance the lane ahead reserves, so the stop needs no more
                // than the acceleration limit and the participant does not overshoot and back up.
                plan.hold[side] = guard.braking_target();
            }
        } else if (plan.waiting[side] && tick - plan.intruded_at[side] >= passing_resume_ticks) {
            plan.waiting[side] = false;
        }
        if (plan.waiting[side] && tick - plan.waiting_since[side] >= passing_timeout_ticks)
            result = PassingEnd::obstructed;
    }
    return result;
}

void PredictivePassing::form_plans(const FloorPlan &floor, const Fleet &fleet, const MotionSettings &motion,
                                   std::uint64_t tick, const SpatialGrid &index, EventLog &log) {
    std::vector<bool> claimed(fleet.movers.size());
    for (std::size_t i = 0; i < fleet.movers.size(); ++i)
        claimed[i] = fleet.controls[i].has_value();
    for (const auto &plan : plans_)
        for (auto id : plan.movers)
            claimed[id] = true;
    // Look far enough ahead to fit two lane changes before the movers meet.
    const float horizon =
        std::max(motion.horizon, (4 * length(fleet.max_half) + formation_room_mm) / motion.speed +
                                     2 * motion.speed / motion.force);
    const float range = 2 * motion.speed * horizon + 2 * length(fleet.max_half);
    std::vector<PassingApproach> approaches;
    approaches.reserve(fleet.movers.size());
    for (const auto &m : fleet.movers)
        approaches.push_back(passing_approach(m));
    auto eligible = [&](std::size_t id) {
        const auto &m = fleet.movers[id];
        return !claimed[id] && m.moving() && !m.held && !m.coordination.corridor_retreating &&
               !m.route.empty() && retry_[id] <= tick;
    };
    for (std::size_t a = 0; a < fleet.movers.size(); ++a) {
        if (!eligible(a))
            continue;
        const auto &first = fleet.movers[a];
        // Oncoming movers whose closest approach, at full speed, would bring them into contact.
        std::vector<std::pair<float, std::size_t>> candidates;
        for (auto b : index.query(
                 {first.body.position - Vec2{range, range}, first.body.position + Vec2{range, range}})) {
            if (b <= a || !eligible(b))
                continue;
            if (const auto time = passing_conflict_time(approaches[a], approaches[b], motion.speed, horizon,
                                                        passing_gap_mm))
                candidates.push_back({*time, b});
        }
        std::sort(candidates.begin(), candidates.end());
        for (const auto &candidate : candidates)
            if (try_form(a, candidate.second, approaches, floor, fleet, tick, index, log)) {
                claimed[a] = claimed[candidate.second] = true;
                break;
            }
    }
}

bool PredictivePassing::try_form(std::size_t a, std::size_t b, const std::vector<PassingApproach> &approaches,
                                 const FloorPlan &floor, const Fleet &fleet, std::uint64_t tick,
                                 const SpatialGrid &index, EventLog &log) {
    constexpr PassingMargins margins{passing_gap_mm, exit_offset_mm, formation_room_mm};
    for (const auto &lanes : passing_lane_candidates(approaches[a], approaches[b], margins)) {
        if (!lanes)
            continue;
        Plan plan;
        plan.movers = {a, b};
        plan.progress_at = tick;
        plan.best_remaining = std::numeric_limits<float>::infinity();
        for (std::size_t side = 0; side < 2; ++side) {
            plan.entry[side] = (*lanes)[side].entry;
            plan.exit[side] = (*lanes)[side].exit;
        }
        if (!lanes_clear(plan, floor, fleet, index))
            continue;
        plan.id = next_plan_++;
        record(EventKind::passing_formed, plan, tick, log);
        plans_.push_back(std::move(plan));
        return true;
    }
    return false;
}

bool PredictivePassing::lanes_clear(Plan &plan, const FloorPlan &floor, const Fleet &fleet,
                                    const SpatialGrid &index) const {
    // Each participant's two legs must stay clear of walls, third movers, traffic zones and
    // the space other plans reserved; the swept areas are recorded in `plan.space`.
    bool clear = true;
    for (std::size_t side = 0; side < 2 && clear; ++side) {
        const auto id = plan.movers[side];
        auto moving = fleet.movers[id].body;
        moving.half_extents += clearance;
        Vec2 from = moving.position;
        for (auto to : {plan.entry[side], plan.exit[side]}) {
            if (!floor.grid().clear(from, to, moving.half_extents)) {
                clear = false;
                break;
            }
            moving.position = from;
            const auto area = swept_box_bounds(from, to, moving.half_extents);
            for (auto other : index.query(area))
                if (other != plan.movers[0] && other != plan.movers[1] &&
                    sweep_translation(moving, to, fleet.movers[other].body))
                    clear = false;
            for (const auto &zone : floor.zones()) {
                Body obstacle;
                obstacle.shape = Shape::box;
                obstacle.motion = Motion::fixed;
                obstacle.position = zone.center;
                obstacle.half_extents = zone.half_extents + clearance;
                if (sweep_translation(moving, to, obstacle))
                    clear = false;
            }
            for (const auto &other : plans_)
                for (const auto &reserved : other.space)
                    if (reserved.overlaps(area))
                        clear = false;
            plan.space.push_back(area);
            from = to;
        }
    }
    return clear;
}
} // namespace i4sim::detail
