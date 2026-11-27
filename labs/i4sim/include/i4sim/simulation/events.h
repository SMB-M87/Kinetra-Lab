#pragma once
#include <kinetra/simulation/math.h>
#include <kinetra/simulation/stall_detector.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace i4sim {
/// What a recorded simulation event reports.
enum class EventKind {
    /// Two movers accepted a predictive passing plan and head for their entry targets.
    passing_formed,
    /// Both participants reached their entry targets; they now head for their exit targets.
    passing_crossing,
    /// A passing plan was removed; `PassingEnd` says why.
    passing_ended,
    /// A mover started touching another mover or a border.
    contact,
    /// A mover began a sustained period without route progress.
    stall_started,
    /// A previously reported stall ended because progress resumed or observation reset.
    stall_ended,
    /// A parked mover started a trip to a checked refuge.
    make_way_started,
    /// The policy could not find a checked way past a parked mover.
    make_way_unavailable,
    /// The request ended and the yielding mover can return when a route clears.
    make_way_ended,
    /// A stalled mover that a neighbor pushed back got the right of way; the second mover is
    /// the pushing neighbor.
    right_of_way_granted,
    /// The right of way ended: its window passed, the mover left its route or the policy
    /// was turned off.
    right_of_way_ended,
};

/// Evidence available when the detector observed the start of a stall: Kinetra's reasons
/// (a stationary blocker, an opposing mover, a neighbor pushing the mover back, oscillation,
/// or none found).
using StallReason = kinetra::StallReason;

/// Diagnosis at the onset of one observed stall episode: the tick at which sustained lack of
/// progress was first reported, the reason, and the index into Simulation::movers() of a
/// stationary blocker, an opposing mover or a pushing neighbor.
using StallDetails = kinetra::StallDiagnosis;

/// Why a passing plan was removed. When several reasons apply in the same tick, the
/// simulation records the first it finds: `disabled`; then for each participant in
/// turn `participant_stopped`, `participant_held`, `traffic_control`; then
/// `obstructed`; then `no_progress`; and only when none applies, `completed`. `goal_changed` is recorded
/// immediately when a participant receives a goal, and `disabled` when a motion change
/// turns passing off.
enum class PassingEnd {
    /// Both participants reached their exit targets after crossing.
    completed,
    /// Predictive passing is off or the steering profile does not support it.
    /// `Simulation::configure_motion` records it at once when a change turns passing off.
    disabled,
    /// A participant has no active route any more.
    participant_stopped,
    /// A participant was held with `Simulation::toggle_mover`.
    participant_held,
    /// Corridor recovery took control of a participant.
    traffic_control,
    /// The summed distance of both participants to their targets did not shrink for
    /// 240 ticks.
    no_progress,
    /// A participant received a goal: a manual command, a scheduling or parking
    /// decision, or a PLANNER change (path planning or heatmap) that plans every route
    /// again.
    goal_changed,
    /// A mover that stands still (idle, unreachable or held) is on a participant's
    /// remaining lane, another mover drives toward a participant along its lane, or a
    /// participant waited 240 ticks for a mover to leave its lane ahead.
    obstructed,
};

/// Why an active make-way request stopped.
enum class MakeWayEnd {
    /// The beneficiary cleared the space it needed.
    cleared,
    /// The beneficiary changed goals, was held, or the operator disabled the policy.
    cancelled,
    /// The trip exceeded its time limit before clearing the space.
    timed_out,
    /// The checked path to the refuge no longer clears an obstruction.
    obstructed,
};

/// One entry of the simulation event log (see `Simulation::record_events`).
///
/// Fields that do not apply to `kind` keep their default value. Positions are in
/// millimeters in blueprint coordinates. Recording an event never changes a decision.
struct SimulationEvent {
    /// Value of `Simulation::ticks()` when the event was recorded: the running tick, or
    /// the last completed tick for a command given between ticks.
    std::uint64_t tick{};
    EventKind kind{};
    /// Passing plan the event belongs to; ids start at 1 and are unique per simulation.
    /// 0 for events outside a passing plan.
    std::uint64_t plan{};
    /// Passing: the two participants. Contact: the two movers, or for a border contact
    /// the mover and the index into `Simulation::borders()`. Stall: first entry is the
    /// affected mover and the second entry is unused. MAKE WAY uses both mover indices.
    std::array<std::size_t, 2> movers{};
    /// `passing_formed`: the entry and exit target of each participant, in the order
    /// of `movers`.
    std::array<kinetra::Vec2, 2> entry{}, exit{};
    /// `passing_ended`: why the plan was removed; empty for every other kind.
    std::optional<PassingEnd> end{};
    /// `contact`: true when the second participant is a border, not a mover.
    bool border{};
    /// `contact`: true when a participant was in a passing plan in that tick.
    bool passing{};
    /// Stall events carry the onset diagnosis, including when the episode ends.
    std::optional<StallDetails> stall{};
    /// Outcome for `make_way_ended` events.
    std::optional<MakeWayEnd> make_way_end{};
};

/// Stable lower-case name of `kind`, for logs, CSV columns and test output.
constexpr std::string_view name(EventKind kind) {
    switch (kind) {
    case EventKind::passing_formed:
        return "passing_formed";
    case EventKind::passing_crossing:
        return "passing_crossing";
    case EventKind::passing_ended:
        return "passing_ended";
    case EventKind::contact:
        return "contact";
    case EventKind::stall_started:
        return "stall_started";
    case EventKind::stall_ended:
        return "stall_ended";
    case EventKind::make_way_started:
        return "make_way_started";
    case EventKind::make_way_unavailable:
        return "make_way_unavailable";
    case EventKind::make_way_ended:
        return "make_way_ended";
    case EventKind::right_of_way_granted:
        return "right_of_way_granted";
    case EventKind::right_of_way_ended:
        return "right_of_way_ended";
    }
    return "unknown";
}

/// Stable lower-case name of a make-way outcome for event export.
constexpr std::string_view name(MakeWayEnd outcome) {
    switch (outcome) {
    case MakeWayEnd::cleared:
        return "cleared";
    case MakeWayEnd::cancelled:
        return "cancelled";
    case MakeWayEnd::timed_out:
        return "timed_out";
    case MakeWayEnd::obstructed:
        return "obstructed";
    }
    return "unknown";
}

/// Stable lower-case name of the diagnosis for event logs.
constexpr std::string_view name(StallReason reason) {
    switch (reason) {
    case StallReason::stationary_blocker:
        return "stationary_blocker";
    case StallReason::opposing_mover:
        return "opposing_mover";
    case StallReason::pushed_back:
        return "pushed_back";
    case StallReason::oscillating:
        return "oscillating";
    case StallReason::unknown:
        return "unknown";
    }
    return "unknown";
}

/// Stable lower-case name of `end`, for logs, CSV columns and test output.
constexpr std::string_view name(PassingEnd end) {
    switch (end) {
    case PassingEnd::completed:
        return "completed";
    case PassingEnd::disabled:
        return "disabled";
    case PassingEnd::participant_stopped:
        return "participant_stopped";
    case PassingEnd::participant_held:
        return "participant_held";
    case PassingEnd::traffic_control:
        return "traffic_control";
    case PassingEnd::no_progress:
        return "no_progress";
    case PassingEnd::goal_changed:
        return "goal_changed";
    case PassingEnd::obstructed:
        return "obstructed";
    }
    return "unknown";
}
} // namespace i4sim
