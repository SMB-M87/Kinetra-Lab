#pragma once
// Counts over a simulation's event log, shared by the pressure suite and the motion
// benchmark, so both report passing outcomes by the same definitions.
#include "i4sim/simulation/events.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <tuple>

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
/// Ticks between two contact starts of the same pair below which both belong to one
/// contact episode.
inline constexpr std::uint64_t contact_episode_gap_ticks = 500;

/// Contact episodes: contact starts of the same mover pair (or mover and border), grouped
/// while less than `gap` ticks lie between one start and the next. A pair that jostles,
/// touching again and again, counts once per episode, however many contacts it starts.
inline std::size_t contact_episodes(std::span<const SimulationEvent> events,
                                    std::uint64_t gap = contact_episode_gap_ticks) {
    std::map<std::tuple<std::size_t, std::size_t, bool>, std::uint64_t> last;
    std::size_t episodes = 0;
    for (const auto &event : events) {
        if (event.kind != EventKind::contact)
            continue;
        const auto [found, inserted] =
            last.try_emplace({event.movers[0], event.movers[1], event.border}, event.tick);
        if (inserted || event.tick - found->second >= gap)
            ++episodes;
        found->second = event.tick;
    }
    return episodes;
}
} // namespace i4sim::test
