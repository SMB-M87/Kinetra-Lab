#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace demo {
/// Demo environments in launcher order. Terrain runs the streamed sandbox.
enum class Scenario { terrain, gravity, maze, doorway };

/// Scenario labels in launcher order.
std::vector<std::string> scenario_labels();
/// The display label, or std::out_of_range for an invalid scenario.
std::string_view scenario_label(Scenario scenario);
/// Resolves a menu index; throws std::out_of_range if no scenario has that index.
Scenario scenario_at(std::size_t index);
/// Resolves a lowercase CLI name; throws std::invalid_argument for unknown names.
Scenario parse_scenario(std::string_view name);
} // namespace demo
