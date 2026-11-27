#include "i4sim/simulation/detail/predictive_passing.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/routing.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <kinetra/simulation/physics.h>

#include <array>
#include <cmath>
#include <vector>

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
/// Ticks the lane ahead of a waiting participant must stay clear before it drives on, so
/// a mover passing close by does not make it start and stop in turn.
constexpr std::uint64_t passing_resume_ticks = 20;
/// Ticks between two predicted positions of another mover in the lane check; at the
/// default 2 mm/tick a mover moves 8 mm, far less than any footprint.
constexpr int prediction_step_ticks = 4;

/// The factory's tuning of the coordinator, in millimeters and ticks.
PassingCoordination coordination(const MotionSettings &motion) {
    return {.margins = {passing_gap_mm, exit_offset_mm, formation_room_mm},
            .prediction = {motion.speed, motion.force, int(std::ceil(motion.horizon)), prediction_step_ticks},
            .clearance = clearance,
            .arrival_distance = arrival_distance_mm,
            .stop_speed = stop_speed,
            .resume_steps = passing_resume_ticks,
            .timeout_steps = passing_timeout_ticks,
            .retry_steps = passing_retry_ticks};
}

/// What a mover is to the coordinator. A parked, idle, unreachable or held mover stands; a
/// mover under traffic control, retreating in a corridor or on a make-way trip follows
/// another control and joins no plan.
PassingMoverState state(const Fleet &fleet, std::size_t id) {
    const auto &m = fleet.movers[id];
    if (!m.moving() || m.held)
        return PassingMoverState::standing;
    if (fleet.controls[id] || m.coordination.corridor_retreating || m.goal == GoalKind::making_way)
        return PassingMoverState::busy;
    return PassingMoverState::free;
}

/// The factory's reason for a plan whose participant stopped being free, in the order the
/// event log documents: for each participant in turn, stopped, held, then traffic control.
PassingEnd unavailable_reason(const Fleet &fleet, const std::array<std::size_t, 2> &movers) {
    for (const auto id : movers) {
        const auto &m = fleet.movers[id];
        if (!m.moving())
            return PassingEnd::participant_stopped;
        if (m.held)
            return PassingEnd::participant_held;
        if (fleet.controls[id] || m.coordination.corridor_retreating)
            return PassingEnd::traffic_control;
        // A make-way trip starts with a new goal, which cancels the plan first; a goal kind
        // set without one still reads as a goal change, though the coordinator counts it as
        // unavailable and both movers then wait passing_retry_ticks.
        if (m.goal == GoalKind::making_way)
            return PassingEnd::goal_changed;
    }
    return PassingEnd::participant_stopped;
}

PassingEnd factory_end(PassingOutcome outcome, PassingEnd cancelled, const Fleet &fleet,
                       const std::array<std::size_t, 2> &movers) {
    switch (outcome) {
    case PassingOutcome::completed:
        return PassingEnd::completed;
    case PassingOutcome::unavailable:
        return unavailable_reason(fleet, movers);
    case PassingOutcome::obstructed:
        return PassingEnd::obstructed;
    case PassingOutcome::no_progress:
        return PassingEnd::no_progress;
    case PassingOutcome::cancelled:
        return cancelled;
    }
    return cancelled;
}

/// Plans the route of a former participant again and resets its progress and control.
void restore_route(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion, std::size_t id) {
    auto &m = fleet.movers[id];
    m.coordination.passing = false;
    m.route = plan_route(floor, fleet, motion, id, m.destination);
    m.lifecycle = m.route.empty() ? MoverLifecycle::unreachable : MoverLifecycle::moving;
    auto &tracking = fleet.progress[id];
    tracking.no_progress = tracking.replan = 0;
    tracking.best_remaining = route_remaining(m.body.position, m.route);
    if (id < fleet.controls.size())
        fleet.controls[id].reset();
}
} // namespace

bool PredictivePassing::enabled(const MotionSettings &motion) {
    return motion.predictive_passing && supports_predictive_passing(motion.profile);
}

std::optional<Vec2> PredictivePassing::target(std::size_t mover) const {
    if (const auto planned = coordinator_.target(mover))
        return planned->point;
    return std::nullopt;
}

std::optional<Simulation::PassingDetails> PredictivePassing::details(std::size_t mover) const {
    // The inspector asks for every mover in every frame; only a participant needs the plans.
    if (!coordinator_.target(mover))
        return std::nullopt;
    for (const auto &plan : coordinator_.plans())
        for (std::size_t side = 0; side < 2; ++side)
            if (plan.movers[side] == mover)
                return Simulation::PassingDetails{plan.id, plan.movers[1 - side], plan.crossing,
                                                  plan.waiting[side], coordinator_.target(mover)->point};
    return std::nullopt;
}

void PredictivePassing::apply(std::span<const PassingEvent> events, PassingEnd cancelled,
                              const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                              std::uint64_t tick, EventLog &log) {
    for (const auto &event : events) {
        if (log.recording()) {
            SimulationEvent entry{.tick = tick, .plan = event.plan, .movers = event.movers};
            switch (event.kind) {
            case PassingEventKind::formed:
                entry.kind = EventKind::passing_formed;
                entry.entry = {event.lanes[0].entry, event.lanes[1].entry};
                entry.exit = {event.lanes[0].exit, event.lanes[1].exit};
                break;
            case PassingEventKind::crossing:
                entry.kind = EventKind::passing_crossing;
                break;
            case PassingEventKind::ended:
                entry.kind = EventKind::passing_ended;
                entry.end = factory_end(*event.outcome, cancelled, fleet, event.movers);
                break;
            }
            log.add(entry);
        }
        if (event.kind == PassingEventKind::ended)
            for (const auto id : event.movers)
                restore_route(floor, fleet, motion, id);
    }
}

void PredictivePassing::cancel(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                               std::size_t mover, std::uint64_t tick, EventLog &log) {
    // The coordinator's cancel also ends a retry wait, for a new occupant of a slot; a factory
    // mover keeps its index, so a new goal outside a plan must not cut the wait short.
    if (!coordinator_.target(mover))
        return;
    apply(coordinator_.cancel(mover), PassingEnd::goal_changed, floor, fleet, motion, tick, log);
}

void PredictivePassing::disable(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                                std::uint64_t tick, EventLog &log) {
    apply(coordinator_.clear(), PassingEnd::disabled, floor, fleet, motion, tick, log);
}

void PredictivePassing::coordinate(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
                                   std::uint64_t tick, EventLog &log) {
    if (!enabled(motion)) {
        disable(floor, fleet, motion, tick, log);
        return;
    }
    std::vector<PassingMover> movers;
    movers.reserve(fleet.movers.size());
    for (std::size_t id = 0; id < fleet.movers.size(); ++id) {
        const auto &m = fleet.movers[id];
        // A participant's route is its original one, overridden by its plan, so it gives no
        // heading: a mover whose plan ends in this step forms its next plan in the next step,
        // from the route planned again when the plan ended.
        const bool in_plan = coordinator_.target(id).has_value();
        const Vec2 heading =
            in_plan || m.route.empty() ? Vec2{} : normalized(m.route.front() - m.body.position);
        movers.push_back({m.body, heading, m.destination, state(fleet, id)});
    }
    // Lanes keep to the floor and stay out of every traffic zone, whether or not its
    // admission policy is on; the coordinator checks the other movers and plans itself.
    const auto lanes_clear = [&](std::size_t, Vec2 from, Vec2 to, Vec2 half) {
        if (!floor.grid().clear(from, to, half))
            return false;
        Body moving;
        moving.shape = Shape::box;
        moving.position = from;
        moving.half_extents = half;
        for (const auto &zone : floor.zones()) {
            Body obstacle;
            obstacle.shape = Shape::box;
            obstacle.motion = Motion::fixed;
            obstacle.position = zone.center;
            obstacle.half_extents = zone.half_extents + clearance;
            if (sweep_translation(moving, to, obstacle))
                return false;
        }
        return true;
    };
    const auto events = coordinator_.step(movers, coordination(motion), lanes_clear);
    apply(events, PassingEnd::goal_changed, floor, fleet, motion, tick, log);
    for (const auto &plan : coordinator_.plans())
        for (const auto id : plan.movers) {
            fleet.movers[id].coordination.passing = true;
            const auto target = *coordinator_.target(id);
            fleet.controls[id] = TrafficControl{passing_control, target.point, target.waiting};
        }
}
} // namespace i4sim::detail
