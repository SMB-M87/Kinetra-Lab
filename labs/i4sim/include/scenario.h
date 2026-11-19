#pragma once
#include "simulation.h"
#include <cstdint>
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
/// How a scenario repeats once its movers have arrived.
enum class ScenarioRepeat {
    /// Not at all: the movers stay at their goals.
    none,
    /// The goal movers drive back and forth between their start and their goal
    /// (`Simulation::shuttle`); the simulation turns them.
    shuttle,
    /// The scene starts again from its blueprint once every mover has stood still for
    /// `shuttle_pause_ticks`. The session rebuilds the simulation when `RestartTimer` says
    /// so; for a scene that cannot drive back, such as movers that start inside a corridor.
    restart,
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
    /// How the scene repeats once its movers have arrived.
    ScenarioRepeat repeat{ScenarioRepeat::none};
};
/// Reads the `scenario` section of the blueprint at `path`; std::nullopt when there is none.
/// Property names are case-insensitive like the rest of the blueprint, but unknown names
/// inside the section are rejected, so a misspelled setting cannot be silently ignored.
/// Throws std::runtime_error with the path and the problem.
std::optional<Scenario> load_scenario(const std::filesystem::path &path);
/// Configures `simulation` as `scenario` describes and sends the goals in order, as
/// shuttles when `scenario.repeat` is `ScenarioRepeat::shuttle`. Leaves
/// the run state unchanged. Throws std::runtime_error when a goal names no mover, names several,
/// or is rejected by Simulation::move_to or Simulation::shuttle; the simulation may then hold part of the
/// setup, so the caller discards it.
void apply_scenario(const Scenario &scenario, Simulation &simulation);
/// Decides when a `ScenarioRepeat::restart` scene starts again: once every mover of the
/// simulation, not only those with a scenario goal, has stood idle for `shuttle_pause_ticks`
/// ticks. A scene whose movers keep working (the workload on) therefore never restarts.
/// Use one timer per simulation.
class RestartTimer {
  public:
    /// Observes `simulation` after it advanced; true when the scene is due to restart. The
    /// pause counts from the tick of the first call that saw every mover idle, and a call
    /// that sees a mover under way starts it over; calling it after every advance is enough.
    /// Called once per frame, the pause can last up to one frame of ticks longer.
    bool due(const Simulation &simulation);

  private:
    /// Tick at which every mover was first seen idle; empty while one is under way.
    std::optional<std::uint64_t> settled_;
};
} // namespace i4sim
