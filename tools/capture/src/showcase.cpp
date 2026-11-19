#include "showcase.h"

#include "stage.h"

#include "../../../labs/i4sim/include/controller.h"
#include "../../../labs/i4sim/include/scenario.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace capture {
namespace {
/// Longest cycle the measurement waits for, in seconds of steps; the showcase scenes take
/// about a quarter of it.
constexpr double longest_cycle_seconds = 120;
} // namespace

int showcase_cycle_steps(const std::filesystem::path &blueprint) {
    const auto scenario = i4sim::load_scenario(blueprint);
    if (!scenario || scenario->repeat == i4sim::ScenarioRepeat::none)
        throw std::invalid_argument(blueprint.string() + " has no repeating scenario");
    const int departures_per_cycle = scenario->repeat == i4sim::ScenarioRepeat::shuttle ? 2 : 1;

    i4sim::FactoryController controller(capture_seed, false, i4sim_updates_per_second);
    if (!controller.load(blueprint))
        throw std::runtime_error(controller.message());
    kinetra::Camera2D camera;
    // The session's frame without open panels: begin, no modal pause, input and advance.
    const auto advance = [&](const kinetra::Input &input) {
        controller.begin_frame(Stage::step_seconds);
        controller.suspend(false);
        controller.handle_input(Stage::step_seconds, input, camera,
                                {.pointer_captured = false, .debug_mode = true});
    };
    kinetra::Input space;
    space.pressed[std::size_t(kinetra::Key::space)] = space.held[std::size_t(kinetra::Key::space)] = true;
    advance(space);

    // A restart builds a new simulation whose movers set off at once, so both repeat modes
    // end a cycle with a departure after a standstill.
    bool standing = false;
    int departures = 0;
    const auto longest = static_cast<int>(longest_cycle_seconds / Stage::step_seconds);
    for (int step = 0; step < longest; ++step) {
        advance({});
        const auto &movers = controller.simulation().movers();
        const bool moving =
            std::any_of(movers.begin(), movers.end(), [](const auto &mover) { return mover.moving(); });
        if (standing && moving && ++departures == departures_per_cycle)
            return step;
        standing = !moving;
    }
    throw std::runtime_error(blueprint.string() + " does not complete a cycle within " +
                             std::to_string(static_cast<int>(longest_cycle_seconds)) + " seconds");
}
} // namespace capture
