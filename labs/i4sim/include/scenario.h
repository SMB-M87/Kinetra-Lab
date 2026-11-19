#pragma once
#include "simulation.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace i4sim {
/// A manual goal for one mover of a scenario.
struct ScenarioGoal {
    /// The mover's `id` as written in the blueprint's `movers` array, without the model
    /// prefix that load_blueprint adds ("west", not "APM4220_west").
    std::string mover;
    /// Center of the destination in blueprint millimeters (the coordinates move_to takes).
    Vec2 position;
};
/// Optional setup stored in a blueprint's `scenario` section: what a showcase scene needs
/// on top of the factory layout so it runs without manual setup.
struct Scenario {
    /// One line shown while the scene runs; may be empty.
    std::string description;
    /// Steering profile and coordination features; unspecified fields keep their defaults.
    MotionSettings motion;
    /// Whether the automatic product workload runs.
    bool workload{true};
    /// Goals sent to movers in this order when the scenario is applied.
    std::vector<ScenarioGoal> goals;
};
/// Reads the `scenario` section of the blueprint at `path`; std::nullopt when there is none.
/// Property names are case-insensitive like the rest of the blueprint, but unknown names
/// inside the section are rejected, so a misspelled setting cannot be silently ignored.
/// Throws std::runtime_error with the path and the problem.
std::optional<Scenario> load_scenario(const std::filesystem::path &path);
/// Configures `simulation` as `scenario` describes and sends the goals in order. Leaves
/// `paused` unchanged. Throws std::runtime_error when a goal names no mover, names several,
/// or is rejected by Simulation::move_to; the simulation may then hold part of the setup,
/// so the caller discards it.
void apply_scenario(const Scenario &scenario, Simulation &simulation);
} // namespace i4sim
