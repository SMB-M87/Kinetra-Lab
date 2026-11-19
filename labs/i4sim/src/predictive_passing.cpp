#include "predictive_passing.h"
#include "geometry.h"
#include "routing.h"
#include "simulation_constants.h"

#include <algorithm>
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
/// Movers count as oncoming when their headings' cosine is at most this.
constexpr float oncoming_cosine = -0.8f;
/// Share of the maximum speed per tick that counts as progress toward the targets.
constexpr float progress_speed_fraction = 0.05f;
} // namespace

bool PredictivePassing::enabled(const MotionSettings &motion) {
    return motion.predictive_passing && supports_predictive_passing(motion.profile);
}

std::optional<Vec2> PredictivePassing::target(std::size_t mover) const {
    for (const auto &plan : plans_)
        for (std::size_t side = 0; side < 2; ++side)
            if (plan.movers[side] == mover)
                return plan.crossing ? plan.exit[side] : plan.entry[side];
    return std::nullopt;
}

void PredictivePassing::restore_routes(std::array<std::size_t, 2> ids, const FloorPlan &floor, Fleet &fleet,
                                       std::uint64_t tick) {
    for (auto id : ids) {
        auto &m = fleet.movers[id];
        m.coordination.passing = false;
        m.route = plan_route(floor, fleet, id, m.destination);
        m.lifecycle = m.route.empty() ? MoverLifecycle::unreachable : MoverLifecycle::moving;
        auto &tracking = fleet.progress[id];
        tracking.no_progress = tracking.replan = 0;
        tracking.best_remaining = route_remaining(m.body.position, m.route);
        if (id < fleet.controls.size())
            fleet.controls[id].reset();
        if (id < retry_.size())
            retry_[id] = tick + passing_retry_ticks;
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
                            std::uint64_t tick, EventLog &log) {
    record(EventKind::passing_ended, plans_[index], tick, log, reason);
    const auto ids = plans_[index].movers;
    plans_.erase(plans_.begin() + std::ptrdiff_t(index));
    restore_routes(ids, floor, fleet, tick);
}

void PredictivePassing::cancel(const FloorPlan &floor, Fleet &fleet, std::size_t mover, std::uint64_t tick,
                               EventLog &log) {
    for (std::size_t i = 0; i < plans_.size();) {
        const auto ids = plans_[i].movers;
        if (mover != ids[0] && mover != ids[1]) {
            ++i;
            continue;
        }
        end(i, PassingEnd::goal_changed, floor, fleet, tick, log);
    }
}

void PredictivePassing::coordinate(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                                   std::uint64_t tick, const SpatialGrid &index, EventLog &log) {
    retry_.resize(fleet.movers.size());
    advance_plans(floor, fleet, motion, tick, log);
    if (!enabled(motion))
        return;
    form_plans(floor, fleet, motion, tick, index, log);
    for (const auto &plan : plans_)
        for (std::size_t side = 0; side < 2; ++side) {
            const auto id = plan.movers[side];
            fleet.movers[id].coordination.passing = true;
            fleet.controls[id] =
                TrafficControl{passing_control, plan.crossing ? plan.exit[side] : plan.entry[side], false};
        }
}

void PredictivePassing::advance_plans(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                                      std::uint64_t tick, EventLog &log) {
    for (std::size_t i = 0; i < plans_.size();) {
        auto &plan = plans_[i];
        // The first reason found ends the plan; the event log reports it.
        std::optional<PassingEnd> end_reason;
        if (!enabled(motion))
            end_reason = PassingEnd::disabled;
        bool arrived = true;
        float remaining = 0;
        for (std::size_t side = 0; side < 2; ++side) {
            const auto id = plan.movers[side];
            const auto &m = fleet.movers[id];
            const auto target = plan.crossing ? plan.exit[side] : plan.entry[side];
            const float d = distance(m.body.position, target);
            remaining += d;
            arrived = arrived && d < arrival_distance_mm && length(m.body.velocity) < stop_speed;
            if (end_reason)
                continue;
            if (!m.moving())
                end_reason = PassingEnd::participant_stopped;
            else if (m.held)
                end_reason = PassingEnd::participant_held;
            else if (fleet.controls[id].has_value())
                end_reason = PassingEnd::traffic_control;
        }
        if (remaining + std::max(0.0001f, motion.speed * progress_speed_fraction) < plan.best_remaining) {
            plan.best_remaining = remaining;
            plan.progress_at = tick;
        }
        if (!end_reason && tick - plan.progress_at >= passing_timeout_ticks)
            end_reason = PassingEnd::no_progress;
        if (!end_reason && arrived && plan.crossing)
            end_reason = PassingEnd::completed;
        if (end_reason) {
            end(i, *end_reason, floor, fleet, tick, log);
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
    std::vector<Vec2> directions;
    for (const auto &m : fleet.movers)
        directions.push_back(m.route.empty() ? Vec2{} : normalized(m.route.front() - m.body.position));
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
            if (b <= a || !eligible(b) || dot(directions[a], directions[b]) > oncoming_cosine)
                continue;
            const Vec2 p = fleet.movers[b].body.position - first.body.position;
            const Vec2 relative = (directions[a] - directions[b]) * motion.speed;
            const float square = dot(relative, relative);
            if (square <= 0 || dot(p, relative) <= 0)
                continue;
            const float time = dot(p, relative) / square;
            if (time > horizon)
                continue;
            const auto closest = p - relative * time;
            const auto extent = first.body.half_extents + fleet.movers[b].body.half_extents +
                                Vec2{passing_gap_mm, passing_gap_mm};
            if (std::abs(closest.x) < extent.x && std::abs(closest.y) < extent.y)
                candidates.push_back({time, b});
        }
        std::sort(candidates.begin(), candidates.end());
        for (const auto &candidate : candidates)
            if (try_form(a, candidate.second, directions, floor, fleet, tick, index, log)) {
                claimed[a] = claimed[candidate.second] = true;
                break;
            }
    }
}

bool PredictivePassing::try_form(std::size_t a, std::size_t b, const std::vector<Vec2> &directions,
                                 const FloorPlan &floor, const Fleet &fleet, std::uint64_t tick,
                                 const SpatialGrid &index, EventLog &log) {
    const auto &first = fleet.movers[a];
    const auto &second = fleet.movers[b];
    const auto axis = normalized(directions[a] - directions[b]);
    const auto separation = dot(second.body.position - first.body.position, axis);
    const float along = support(first.body, axis) + support(second.body, axis);
    // The formation segments remain longitudinally separated, even when
    // the two movers take different amounts of time to reach their lanes.
    if (separation < 2 * along + formation_room_mm)
        return false;
    const auto midpoint = (first.body.position + second.body.position) / 2;
    // Try both sides, each with the lanes centered on the midpoint or shifted by one lane.
    for (float hand : {-1.f, 1.f}) {
        const auto lateral = perpendicular(axis) * hand;
        const float step =
            std::max(support(first.body, lateral), support(second.body, lateral)) + passing_gap_mm;
        for (float shift : {0.f, step, -step}) {
            Plan plan;
            plan.movers = {a, b};
            plan.progress_at = tick;
            plan.best_remaining = std::numeric_limits<float>::infinity();
            for (std::size_t side = 0; side < 2; ++side) {
                const auto &m = fleet.movers[plan.movers[side]];
                const float sign = side == 0 ? 1.f : -1.f;
                const float lane = sign * (support(m.body, lateral) + passing_gap_mm);
                plan.entry[side] = m.body.position + axis * (sign * along / 2) +
                                   lateral * (dot(midpoint - m.body.position, lateral) + lane + shift);
                plan.exit[side] =
                    midpoint + axis * (sign * (along + exit_offset_mm)) + lateral * (lane + shift);
            }
            if (dot(first.destination - plan.exit[0], axis) < passing_gap_mm ||
                dot(second.destination - plan.exit[1], -axis) < passing_gap_mm)
                continue;
            if (!lanes_clear(plan, floor, fleet, index))
                continue;
            plan.id = next_plan_++;
            record(EventKind::passing_formed, plan, tick, log);
            plans_.push_back(std::move(plan));
            return true;
        }
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
            const auto area = travel_bounds(from, to, moving.half_extents);
            for (auto other : index.query(area))
                if (other != plan.movers[0] && other != plan.movers[1] &&
                    sweep_translation(moving, to - from, fleet.movers[other].body))
                    clear = false;
            for (const auto &zone : floor.zones()) {
                Body obstacle;
                obstacle.shape = Shape::box;
                obstacle.motion = Motion::fixed;
                obstacle.position = zone.center;
                obstacle.half_extents = zone.half_extents + clearance;
                if (sweep_translation(moving, to - from, obstacle))
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
