#pragma once
#include <kinetra/debug_view.h>
namespace i4sim {
/// Modal settings capture every input; layout uses the same coordinates for picking and drawing.
struct FactorySettings {
    bool visible{}, return_requested{}, exit_requested{}, controls_visible{}, debug_controls_visible{};
    kinetra::Vec2 pointer{-1, -1};
    float sim_controls_height{42};
    static float scale(kinetra::Vec2 viewport) {
        return std::min({2.f, viewport.x / 204.f, viewport.y / 418.f});
    }
    float debug_toggle_y() const { return 156 + (controls_visible ? sim_controls_height : 0.f); }
    float actions_y() const { return debug_toggle_y() + 28 + (debug_controls_visible ? 28.f : 0.f); }
    kinetra::Vec2 half_extent() const { return {92, (actions_y() + 50) / 2}; }
    bool update(const kinetra::Input &input, kinetra::Camera2D &camera, bool &statistics, bool &debug,
                float minimum, float maximum) {
        pointer = input.mouse;
        const bool was_visible = visible;
        const float size = scale(camera.viewport);
        const auto point = (input.mouse - camera.viewport / 2) / size + half_extent();
        if (input.hit(kinetra::Key::control) ||
            (!visible && input.clicked && input.mouse.x >= 0 && input.mouse.x < 64 && input.mouse.y >= 0 &&
             input.mouse.y < 24))
            visible = !visible;
        else if (visible && input.clicked && point.x >= 10 && point.x <= 174) {
            if (point.y >= actions_y() - 4 && point.y < actions_y() + 18) {
                if (point.x >= 24.5f && point.x < 77.5f)
                    return_requested = true;
                if (point.x >= 83.5f && point.x < 124.5f)
                    visible = false;
                if (point.x >= 130.5f && point.x < 159.5f)
                    exit_requested = true;
            }
            if (point.y >= 36 && point.y < 58) {
                if (point.x >= 32 && point.x < 81)
                    camera.pixels_per_unit = std::clamp(camera.pixels_per_unit * 1.1f, minimum, maximum);
                if (point.x >= 97 && point.x < 152)
                    camera.pixels_per_unit = std::clamp(camera.pixels_per_unit / 1.1f, minimum, maximum);
            }
            if (point.y >= 64 && point.y < 86)
                statistics = !statistics;
            if (point.y >= 92 && point.y < 114)
                debug = !debug;
            if (point.y >= 124 && point.y < 146)
                controls_visible = !controls_visible;
            else if (point.y >= debug_toggle_y() - 4 && point.y < debug_toggle_y() + 18)
                debug_controls_visible = !debug_controls_visible;
        }
        return was_visible || visible;
    }
    bool update_debug(const kinetra::Input &input, kinetra::Camera2D &camera, kinetra::DebugMenu &menu,
                      std::span<kinetra::DebugOption> options) const {
        menu.screen_origin = {8, 32};
        const bool captured = menu.update(input, camera.viewport, options);
        if (input.mouse.x >= 64 && input.mouse.x < 112 && input.mouse.y >= 0 && input.mouse.y < 24) {
            if (input.clicked)
                menu.visible = !menu.visible;
            return true;
        }
        return captured;
    }
    void draw(kinetra::Runtime &runtime, bool statistics, bool debug, bool) const {
        using namespace kinetra;
        if (!visible) {
            const float width = debug ? 112.f : 64.f;
            runtime.box(runtime.camera.screen_to_world({width / 2, 12}),
                        Vec2{width / 2, 12} / runtime.camera.pixels_per_unit, 0, {0.025f, 0.035f, 0.06f});
            const bool hovered = pointer.x >= 0 && pointer.x < 64 && pointer.y >= 0 && pointer.y < 24;
            draw_text(runtime, "SETTINGS", {8, 8}, 1, hovered ? Color{0.25f, 0.65f, 1.f} : Color{1, 1, 1});
            if (debug) {
                const bool debug_hovered =
                    pointer.x >= 64 && pointer.x < 112 && pointer.y >= 0 && pointer.y < 24;
                draw_text(runtime, "DEBUG", {72, 8}, 1,
                          debug_hovered ? Color{0.25f, 0.65f, 1.f} : Color{1, 1, 1});
            }
            return;
        }
        const float size = scale(runtime.camera.viewport);
        const auto origin = runtime.camera.viewport / 2 - half_extent() * size;
        runtime.box(runtime.camera.screen_to_world(runtime.camera.viewport / 2),
                    half_extent() * size / runtime.camera.pixels_per_unit, 0, {0.025f, 0.035f, 0.06f});
        const auto point = (pointer - origin) / size;
        auto line = [&](std::string_view text, float y, bool interactive = false, float x = 10,
                        float left = 10, float right = 174) {
            const bool hovered = interactive && point.x >= left &&
                                 (point.x < right || (right == 174 && point.x == right)) &&
                                 point.y >= y - 4 && point.y < y + 18;
            draw_text(runtime, text, origin + Vec2{x, y} * size, size,
                      hovered ? Color{0.25f, 0.65f, 1.f} : Color{0.9f, 0.93f, 1});
        };
        auto centered = [&](std::string_view text, float y, bool interactive = true) {
            line(text, y, interactive, (184.f - (text.size() * 6.f - 1.f)) / 2);
        };
        centered("SETTINGS", 12, false);
        line("ZOOM IN", 40, true, 36, 32, 81);
        line("ZOOM OUT", 40, true, 101, 97, 152);
        centered(statistics ? "[X] STATISTICS" : "[ ] STATISTICS", 68);
        centered(debug ? "[X] DEBUG MODE" : "[ ] DEBUG MODE", 96);
        centered(controls_visible ? "HIDE SIM CONTROLS" : "SHOW SIM CONTROLS", 128);
        if (controls_visible) {
            centered("WASD / ARROWS  MOVE", 142, false);
            centered("CLICK SELECT / RIGHT BLOCK", 156, false);
            centered("SHIFT CLICK FREE MOVER GOAL", 170, false);
            centered("SPACE PAUSE / R RESET", 184, false);
        }
        const float debug_y = debug_toggle_y();
        centered(debug_controls_visible ? "HIDE DEBUG CONTROLS" : "SHOW DEBUG CONTROLS", debug_y);
        if (debug_controls_visible) {
            centered("WHEEL  ZOOM", debug_y + 14, false);
            centered("DEBUG PAGES: VIEW / MOTION", debug_y + 28, false);
        }
        const float actions = actions_y();
        line("LAUNCHER", actions, true, 27.5f, 24.5f, 77.5f);
        line("RESUME", actions, true, 86.5f, 83.5f, 124.5f);
        line("EXIT", actions, true, 133.5f, 130.5f, 159.5f);
        centered("CTRL  SETTINGS", actions + 28, false);
    }
};
} // namespace i4sim
