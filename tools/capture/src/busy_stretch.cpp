#include "busy_stretch.h"

#include "stage.h"

#include "../../../labs/i4sim/include/controller.h"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <string>

namespace capture {
void apply_menu_clicks(i4sim::FactoryController &controller, std::span<const MenuClick> clicks) {
    for (const auto &click : clicks) {
        // Built anew for every click: a click opens and closes sections and lists.
        auto menu = controller.menu();
        if (click.row >= menu.size() || menu[click.row].label != click.label)
            throw std::runtime_error(
                "The i4sim debug menu changed: row " + std::to_string(click.row) + " is " +
                (click.row < menu.size() ? "'" + std::string(menu[click.row].label) + "'" : "missing") +
                ", not '" + std::string(click.label) + "'");
        menu[click.row].action();
    }
}

std::vector<int> driving_movers(const std::filesystem::path &blueprint, int steps) {
    if (steps < 0)
        throw std::invalid_argument("The number of steps must not be negative");
    i4sim::FactoryController controller(capture_seed, false, i4sim_updates_per_second);
    if (!controller.load(blueprint))
        throw std::runtime_error(controller.message());
    apply_menu_clicks(controller, i4sim_motion_clicks);
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

    std::vector<int> driving;
    driving.reserve(std::size_t(steps));
    for (int step = 0; step < steps; ++step) {
        advance({});
        const auto &movers = controller.simulation().movers();
        driving.push_back(static_cast<int>(std::count_if(movers.begin(), movers.end(), [](const auto &mover) {
            return kinetra::length(mover.body.velocity) > driving_speed;
        })));
    }
    return driving;
}

int busiest_window_start(std::span<const int> activity, int window, int stride) {
    if (window <= 0 || stride <= 0 || activity.size() < std::size_t(window))
        throw std::invalid_argument(
            "The busiest window needs a positive window and stride and enough entries");
    const auto size = std::size_t(window);
    long long sum = std::accumulate(activity.begin(), activity.begin() + window, 0LL);
    long long best = sum;
    std::size_t best_start = 0;
    for (std::size_t start = 1; start + size <= activity.size(); ++start) {
        sum += activity[start + size - 1] - activity[start - 1];
        if (start % std::size_t(stride) == 0 && sum > best) {
            best = sum;
            best_start = start;
        }
    }
    return static_cast<int>(best_start);
}
} // namespace capture
