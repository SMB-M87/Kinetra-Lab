#include "../../labs/settings.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        for (float controls_height : {42.f, 28.f})
            for (auto viewport : {kinetra::Vec2{1280, 800}, kinetra::Vec2{160, 120}}) {
                lab::Settings settings{.sim_controls_height = controls_height};
                kinetra::Camera2D camera;
                camera.viewport = viewport;
                camera.pixels_per_unit = 25;
                bool statistics = false, debug = false;
                auto update = [&](const kinetra::Input &input) {
                    return settings.update(input, camera, statistics, debug, 25, 50);
                };
                auto click = [&](float x, float y) {
                    kinetra::Input input;
                    input.clicked = true;
                    input.mouse = viewport / 2 + (kinetra::Vec2{x, y} - settings.half_extent()) *
                                                     lab::Settings::scale(viewport);
                    return update(input);
                };
                kinetra::Input input;
                input.pressed[std::size_t(kinetra::Key::space)] = true;
                input.pressed[std::size_t(kinetra::Key::m)] = true;
                require(!update(input) && !settings.visible && !settings.return_requested,
                        "Space or M activated settings");
                input = {};
                input.pressed[std::size_t(kinetra::Key::h)] = true;
                input.pressed[std::size_t(kinetra::Key::escape)] = true;
                require(!update(input), "H or Escape activated settings");
                input = {};
                input.clicked = true;
                input.mouse = {10, 10};
                require(update(input) && settings.visible, "Settings button did not open modal");
                require(!settings.controls_visible, "Controls should start hidden");
                const auto collapsed = settings.half_extent();
                click(92, 134);
                require(settings.half_extent().y > collapsed.y, "Controls did not expand the panel");
                require(settings.controls_visible, "Show Controls did not expand help");
                require(settings.debug_toggle_y() == 156 + controls_height &&
                            settings.half_extent().y == collapsed.y + controls_height / 2,
                        "Sim controls reserved the wrong number of rows");
                click(92, 134);
                require(!settings.controls_visible, "Hide Controls did not collapse help");
                click(92, settings.debug_toggle_y());
                require(settings.debug_controls_visible && !settings.controls_visible,
                        "Debug controls must expand independently");
                click(92, 134);
                require(settings.controls_visible && settings.debug_controls_visible,
                        "Both control sections must expand together");
                const float last_sim_y = controls_height == 42 ? 184.f : 170.f;
                require(settings.debug_toggle_y() - last_sim_y == 14,
                        "Sim controls must end one row before the debug toggle");
                require(settings.actions_y() - (settings.debug_toggle_y() + 42) == 28,
                        "Debug controls changed the spacing before launcher actions");
                click(92, settings.debug_toggle_y());
                require(!settings.debug_controls_visible && settings.controls_visible,
                        "Hiding debug controls collapsed sim controls");
                click(92, 134);
                require(click(56, 46) && camera.pixels_per_unit > 25, "Zoom in failed");
                for (int i = 0; i < 30; ++i)
                    click(56, 46);
                require(camera.pixels_per_unit == 50, "Zoom exceeded maximum");
                for (int i = 0; i < 30; ++i)
                    click(124, 46);
                require(camera.pixels_per_unit == 25, "Zoom exceeded minimum");
                click(40, 74);
                click(40, 102);
                require(statistics && debug, "Settings toggles failed");
                require(click(106, settings.actions_y()) && !settings.visible,
                        "Resume click was not captured");
                input = {};
                input.clicked = true;
                const float debug_scale = kinetra::DebugMenu::scale(viewport);
                input.mouse = {viewport.x - 180 * debug_scale, 36 * debug_scale};
                require(!update(input) && !settings.visible,
                        "Settings button overlaps debug controls in a small viewport");
                input = {};
                input.pressed[std::size_t(kinetra::Key::control)] = true;
                require(update(input) && settings.visible, "CTRL did not open settings");
                require(update(input) && !settings.visible, "CTRL closing leaked input");
                update(input);
                click(50, settings.actions_y());
                require(settings.return_requested, "Launcher request missing");
                click(146, settings.actions_y());
                require(settings.exit_requested, "Exit request missing");
            }
        std::cout << "PASS settings controls, capture, zoom bounds and resized picking\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
