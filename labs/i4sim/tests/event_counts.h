#pragma once
// Counts over a simulation's event log, shared by the pressure suite and the motion
// benchmark, so both report passing outcomes by the same definitions.
#include "events.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

namespace i4sim::test {
/// Counts the events of `kind`, and for `passing_ended` only those that ended with `end`.
inline std::size_t count(std::span<const SimulationEvent> events, EventKind kind,
                         std::optional<PassingEnd> end = std::nullopt) {
    return std::size_t(std::count_if(events.begin(), events.end(), [&](const SimulationEvent &event) {
        return event.kind == kind && (!end || event.end == end);
    }));
}

/// Passing plans that ended for any reason other than completing.
inline std::size_t cancelled_plans(std::span<const SimulationEvent> events) {
    return count(events, EventKind::passing_ended) -
           count(events, EventKind::passing_ended, PassingEnd::completed);
}

/// Contacts that started while a participant was in a passing plan.
inline std::size_t passing_contacts(std::span<const SimulationEvent> events) {
    return std::size_t(std::count_if(events.begin(), events.end(), [](const SimulationEvent &event) {
        return event.kind == EventKind::contact && event.passing;
    }));
}
} // namespace i4sim::test
