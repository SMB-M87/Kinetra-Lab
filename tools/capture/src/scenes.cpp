#include "scenes.h"

#include "showcase.h"
#include "stage.h"

#include "../../../labs/i4sim/include/blueprint.h"

#include <kinetra/launcher.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace capture {
namespace {
using kinetra::Input;
using kinetra::Key;
using kinetra::Vec2;
using launcher::Variant;

/// Window size of a screenshot, and of a clip: clips are rendered at their final size, so
/// the bitmap font stays sharp in the GIF.
constexpr int shot_width = 1920, shot_height = 1080, clip_width = 960, clip_height = 540;
/// Length of a clip in frames: four seconds at 15 frames per second.
constexpr int clip_frames = 60;
/// Clip frames per second.
constexpr int clip_fps = Stage::clip_fps;
/// Seconds a session runs before it is captured, so agents and enemies are under way. Dotio's
/// enemies first have to find the player; i4sim's movers need time to spread over the hall.
constexpr double warm_up_seconds = 2, dotio_warm_up_seconds = 8, i4sim_warm_up_seconds = 20;
/// Debug-menu rows. Dotio: the last of its rows once PATHS (row 3) has opened its four
/// filters, INVULNERABLE. i4sim: the LAYERS heading while SIMULATION and its four rows are open.
constexpr std::size_t dotio_last_row = 12, i4sim_layers_heading_row = 5;
/// Wheel notches the Dotio debug scenes zoom out, for a wider view of the agents.
constexpr float dotio_debug_zoom_out = -3;

/// A point at a fraction of the window, for clicks and aiming.
Vec2 at(const Stage &stage, float x, float y) {
    return {stage.viewport().x * x, stage.viewport().y * y};
}

Input held(std::initializer_list<Key> keys) {
    Input input;
    for (const Key key : keys)
        input.held[std::size_t(key)] = true;
    return input;
}

// Launcher pages.

void launcher_menu(Stage &stage, bool difficulty_page) {
    kinetra::LaunchMenu menu(launcher_choices());
    Input input;
    input.mouse = stage.viewport() - Vec2{2, 2};
    menu.update(input, stage.viewport());
    if (difficulty_page)
        for (const Key key : {Key::down, Key::enter}) {
            Input press = input;
            press.pressed[std::size_t(key)] = true;
            menu.update(press, stage.viewport());
        }
    menu.draw(stage.runtime());
    stage.write_frame();
}

void launcher_page(Stage &stage) {
    launcher_menu(stage, false);
}

void launcher_difficulty(Stage &stage) {
    launcher_menu(stage, true);
}

void launcher_loadscreen(Stage &stage) {
    stage.start(Variant::i4sim);
    stage.step();
    stage.shot();
}

// Demo: starts in Debug Mode with every visual layer on; the normal view turns it off.

/// The player walks to a point on the right, then to one on the lower left.
Input demo_walk(Stage &stage, int frame) {
    Input input;
    if (frame == 0 || frame == clip_frames / 2) {
        stage.aim(frame == 0 ? at(stage, .72f, .38f) : at(stage, .32f, .66f));
        input.clicked = input.mouse_down = true;
    }
    return input;
}

void demo_start(Stage &stage, bool debug) {
    stage.start(Variant::demo);
    if (!debug)
        stage.toggle_setting(SettingsRow::debug_mode);
    else
        stage.press(Key::backquote);
    stage.wait(warm_up_seconds);
}

void demo(Stage &stage) {
    demo_start(stage, false);
    stage.shot();
}

void demo_debug(Stage &stage) {
    demo_start(stage, true);
    stage.shot();
}

void demo_clip(Stage &stage) {
    demo_start(stage, false);
    stage.record(clip_frames, [&](int frame) { return demo_walk(stage, frame); });
}

void demo_debug_clip(Stage &stage) {
    demo_start(stage, true);
    stage.record(clip_frames, [&](int frame) { return demo_walk(stage, frame); });
}

// Dotio: debug is opt-in; the debug view turns on Debug Mode and every visual row.

/// The player runs a square through the corridors, one second per side, and fires in
/// half-second bursts, so mana lasts and bolts are in the air.
Input dotio_play(int frame) {
    constexpr Key route[]{Key::d, Key::s, Key::a, Key::w};
    Input input = held({route[(frame / clip_fps) % 4]});
    input.mouse_down = frame % clip_fps < clip_fps / 2;
    return input;
}

void dotio_start(Stage &stage, bool debug) {
    stage.start(Variant::dotio);
    if (debug) {
        stage.toggle_setting(SettingsRow::debug_mode);
        stage.press(Key::backquote);
        // Every row but GHOST CAMERA (row 0). PATHS opens its four filters right below it,
        // so the rows after it are clicked in their new places.
        for (std::size_t row = 1; row <= dotio_last_row; ++row)
            stage.click_debug_row(row);
        Input zoom;
        zoom.wheel = dotio_debug_zoom_out;
        stage.point_at(at(stage, .7f, .5f));
        stage.step(zoom);
    }
    stage.point_at(at(stage, .7f, .45f));
    stage.play(static_cast<int>(dotio_warm_up_seconds * clip_fps), dotio_play);
}

void dotio(Stage &stage) {
    dotio_start(stage, false);
    stage.shot();
}

void dotio_debug(Stage &stage) {
    dotio_start(stage, true);
    stage.shot();
}

void dotio_settings(Stage &stage) {
    stage.start(Variant::dotio);
    stage.wait(warm_up_seconds);
    stage.open_settings();
    stage.click_setting(SettingsRow::sim_controls);
    stage.point_at(stage.viewport() - Vec2{2, 2});
    stage.shot();
}

/// Continues dotio_play() where the warm-up stopped.
Input dotio_clip_play(int frame) {
    return dotio_play(static_cast<int>(dotio_warm_up_seconds * clip_fps) + frame);
}

void dotio_clip(Stage &stage) {
    dotio_start(stage, false);
    stage.record(clip_frames, dotio_clip_play);
}

void dotio_debug_clip(Stage &stage) {
    dotio_start(stage, true);
    stage.record(clip_frames, dotio_clip_play);
}

// i4sim: the MAS-demonstrator blueprint, running; Debug Mode is on at the start.

void i4sim_start(Stage &stage, bool debug, bool layers) {
    stage.start_i4sim("MAS-demonstrator.json");
    if (!debug)
        stage.toggle_setting(SettingsRow::debug_mode);
    stage.press(Key::space);
    stage.wait(i4sim_warm_up_seconds);
    if (debug) {
        stage.press(Key::backquote);
        if (layers)
            stage.click_debug_row(i4sim_layers_heading_row);
        stage.point_at(stage.viewport() - Vec2{2, 2});
    }
}

void i4sim(Stage &stage) {
    i4sim_start(stage, false, false);
    stage.shot();
}

void i4sim_debug(Stage &stage) {
    // The LAYERS section with every part and swatch; it fits the screenshot's height.
    i4sim_start(stage, true, true);
    stage.shot();
}

Input no_input(int) {
    return {};
}

/// Doubles the update rate twice with E, to 800 ticks per second: at the default 200 the
/// movers of the MAS demonstrator cover only a few pixels in a four-second clip.
void i4sim_fast_forward(Stage &stage) {
    stage.press(Key::e);
    stage.press(Key::e);
}

void i4sim_clip(Stage &stage) {
    i4sim_start(stage, false, false);
    i4sim_fast_forward(stage);
    stage.record(clip_frames, no_input);
}

void i4sim_debug_clip(Stage &stage) {
    // SIMULATION stays open: the full LAYERS section is taller than a clip.
    i4sim_start(stage, true, false);
    i4sim_fast_forward(stage);
    stage.record(clip_frames, no_input);
}

// i4sim showcase scenes: Debug Mode and every visual layer on, as the session starts, the
// debug menu closed, at the session's own rate.

/// Records one cycle of the showcase scene in `showcase/<scene>.json`. The clip ends in the
/// last standstill before the movers set off again (showcase_cycle_steps), so the GIF loops
/// as the application repeats the scene.
void i4sim_showcase(Stage &stage, std::string_view scene) {
    const auto file = std::filesystem::path("showcase") / (std::string(scene) + ".json");
    stage.start_i4sim(file.string());
    const int frames =
        showcase_cycle_steps(i4sim::default_blueprint_directory() / file) / Stage::steps_per_frame;
    stage.press(Key::space);
    stage.record(frames, no_input);
}

Scene shot(std::string_view name, void (*script)(Stage &)) {
    return {name, SceneKind::screenshot, shot_width, shot_height, script};
}

Scene clip(std::string_view name, void (*script)(Stage &)) {
    return {name, SceneKind::clip, clip_width, clip_height, script};
}
} // namespace

const std::vector<Scene> &scenes() {
    static const std::vector<Scene> all{
        shot("launcher", launcher_page),
        shot("launcher-difficulty", launcher_difficulty),
        shot("launcher-loadscreen", launcher_loadscreen),
        shot("demo", demo),
        shot("demo-debug", demo_debug),
        clip("demo-clip", demo_clip),
        clip("demo-debug-clip", demo_debug_clip),
        shot("dotio", dotio),
        shot("dotio-debug", dotio_debug),
        shot("dotio-settings", dotio_settings),
        clip("dotio-clip", dotio_clip),
        clip("dotio-debug-clip", dotio_debug_clip),
        shot("i4sim", i4sim),
        shot("i4sim-debug", i4sim_debug),
        clip("i4sim-clip", i4sim_clip),
        clip("i4sim-debug-clip", i4sim_debug_clip),
        clip("i4sim-showcase-junction-crossing-clip",
             [](Stage &stage) { i4sim_showcase(stage, "junction-crossing"); }),
        clip("i4sim-showcase-corridor-passing-clip",
             [](Stage &stage) { i4sim_showcase(stage, "corridor-passing"); }),
        clip("i4sim-showcase-corridor-recovery-clip",
             [](Stage &stage) { i4sim_showcase(stage, "corridor-recovery"); }),
        clip("i4sim-showcase-predictive-passing-clip",
             [](Stage &stage) { i4sim_showcase(stage, "predictive-passing"); }),
        clip("i4sim-showcase-h-crossing-clip", [](Stage &stage) { i4sim_showcase(stage, "h-crossing"); }),
    };
    return all;
}

std::vector<const Scene *> select_scenes(std::span<const std::string> names) {
    std::vector<const Scene *> selected;
    if (names.empty()) {
        for (const auto &scene : scenes())
            selected.push_back(&scene);
        return selected;
    }
    for (const auto &name : names) {
        const auto found = std::find_if(scenes().begin(), scenes().end(),
                                        [&](const Scene &scene) { return scene.name == name; });
        if (found == scenes().end())
            throw std::invalid_argument("Unknown scene '" + name + "'");
        selected.push_back(&*found);
    }
    return selected;
}
} // namespace capture
