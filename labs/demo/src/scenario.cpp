#include "demo/scenario.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace demo {
namespace {
struct Entry {
    Scenario scenario;
    std::string_view name;
    std::string_view label;
};

constexpr std::array entries{Entry{Scenario::terrain, "terrain", "TERRAIN"},
                             Entry{Scenario::gravity, "gravity", "GRAVITY"},
                             Entry{Scenario::maze, "maze", "MAZE"},
                             Entry{Scenario::doorway, "doorway", "DOORWAY"},
                             Entry{Scenario::corridor, "corridor", "CORRIDOR"},
                             Entry{Scenario::crossing, "crossing", "CROSSING"},
                             Entry{Scenario::circle, "circle", "CIRCLE"},
                             Entry{Scenario::bottleneck, "bottleneck", "BOTTLENECK"},
                             Entry{Scenario::chaos, "chaos", "CHAOS"},
                             Entry{Scenario::fleet, "fleet", "FLEET"},
                             Entry{Scenario::swarm, "swarm", "SWARM"}};
} // namespace

std::vector<std::string> scenario_labels() {
    std::vector<std::string> labels;
    labels.reserve(entries.size());
    for (const auto &entry : entries)
        labels.emplace_back(entry.label);
    return labels;
}

std::string_view scenario_label(Scenario scenario) {
    return entries.at(static_cast<std::size_t>(scenario)).label;
}

Scenario scenario_at(std::size_t index) {
    return entries.at(index).scenario;
}

Scenario parse_scenario(std::string_view name) {
    const auto entry = std::ranges::find(entries, name, &Entry::name);
    if (entry == entries.end())
        throw std::invalid_argument(
            "Demo scenario must be terrain, gravity, maze, doorway, corridor, crossing, "
            "circle, bottleneck, chaos, fleet or swarm");
    return entry->scenario;
}
} // namespace demo
