#pragma once
#include <kinetra/simulation/math.h>

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
};

/// Why a passing plan was removed. When several reasons apply in the same tick, the
/// simulation records the first it finds: `disabled`; then for each participant in
/// turn `participant_stopped`, `participant_held`, `traffic_control`; then
/// `no_progress`; and only when none applies, `completed`. `goal_changed` is recorded
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
    /// 0 for contacts.
    std::uint64_t plan{};
    /// Passing: the two participants. Contact: the two movers, or for a border contact
    /// the mover and the index into `Simulation::borders()`.
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
    }
    return "unknown";
}
} // namespace i4sim
