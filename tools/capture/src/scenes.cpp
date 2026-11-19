#include "scenes.h"

#include "dotio_player.h"
#include "showcase.h"
#include "stage.h"

#include "../../../labs/i4sim/include/blueprint.h"

#include <kinetra/launcher.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
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
/// Length of the Dotio debug clip and the i4sim clips: twelve seconds. Long enough for a
/// Dotio enemy to run for shelter, reload and come back, and for an i4sim mover to cross
/// its floor at the session's own speed.
constexpr int long_clip_frames = 180;
/// Clip frames per second.
constexpr int clip_fps = Stage::clip_fps;
/// Seconds a session runs before it is captured, so agents and enemies are under way. Dotio's
/// enemies first have to find the player.
constexpr double warm_up_seconds = 2, dotio_warm_up_seconds = 8;
/// Seconds the i4sim floor runs before it is captured: ticks 23200 to 25600 are the busiest
/// stretch of the captured blueprint with capture_seed and the captured motion settings,
/// with 12 to 18 of its 29 movers driving and hardly any waiting.
constexpr double i4sim_warm_up_seconds = 116;
/// Wheel notches the Dotio debug scenes zoom out: the screenshot for a wider view of the
/// agents, the clip far enough to show the whole area the enemies spawn in.
constexpr float dotio_debug_zoom_out = -3, dotio_debug_clip_zoom_out = -9;
/// Longest the Dotio debug clip waits for an enemy to recover before it records anyway, in
/// seconds. A recovery needs nine shots at the player; it usually comes within twenty.
constexpr int dotio_recovery_wait_seconds = 60;
/// How far inside the window that enemy has to be, as a share of the window's height from
/// every edge: it runs for shelter, and from the edge it would leave the clip at once.
constexpr float dotio_recovery_margin = 0.2f;

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

// Dotio: debug is opt-in; the debug view turns on Debug Mode and every visual row. The AI is
// not seeded, so the player is scripted against the world of the running session
// (dotio_player.h) instead of along a fixed timeline.

/// Aims the pointer at the enemy the scripted player would shoot (dotio_aim) and adds the
/// fire button to `input` for the first half of every second, so mana lasts and bolts are
/// in the air. Without an enemy in sight the pointer rests and the player holds fire.
Input dotio_shoot(Stage &stage, int frame, Input input) {
    if (const auto aim = dotio_aim(stage.dotio(), stage.runtime().camera)) {
        stage.aim(*aim);
        input.mouse_down = frame % clip_fps < clip_fps / 2;
    }
    return input;
}

/// The four directions the scripted player moves in, in turn.
constexpr Key dotio_route[]{Key::d, Key::s, Key::a, Key::w};

/// The player runs a square through the corridors, one second per side, and shoots at the
/// enemies it sees.
Input dotio_play(Stage &stage, int frame) {
    return dotio_shoot(stage, frame, held({dotio_route[(frame / clip_fps) % 4]}));
}

/// The player of the debug clip holds its ground, so paths and sight lines stay readable:
/// every two seconds it sidesteps for half a second, in turn right, down, left and up.
Input dotio_sidestep(int frame) {
    constexpr int period = 2 * clip_fps, step = clip_fps / 2;
    return frame % period < step ? held({dotio_route[(frame / period) % 4]}) : Input{};
}

/// Turns on Debug Mode and every visual row of the debug menu, and zooms `notches` wheel
/// notches (negative zooms out). Leaves the menu open and the pointer in the field.
void dotio_debug_view(Stage &stage, float notches) {
    stage.toggle_setting(SettingsRow::debug_mode);
    stage.press(Key::backquote);
    for (std::size_t row = dotio_first_debug_row; row <= dotio_last_debug_row; ++row)
        stage.click_debug_row(row);
    Input zoom;
    zoom.wheel = notches;
    stage.point_at(at(stage, .7f, .5f));
    stage.step(zoom);
}

void dotio_start(Stage &stage, bool debug) {
    stage.start_dotio();
    if (debug)
        dotio_debug_view(stage, dotio_debug_zoom_out);
    stage.point_at(at(stage, .7f, .45f));
    stage.play(static_cast<int>(dotio_warm_up_seconds * clip_fps),
               [&](int frame) { return dotio_play(stage, frame); });
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

void dotio_clip(Stage &stage) {
    dotio_start(stage, false);
    // Continues dotio_play() where the warm-up stopped.
    const int played = static_cast<int>(dotio_warm_up_seconds * clip_fps);
    stage.record(clip_frames, [&](int frame) { return dotio_play(stage, played + frame); });
}

/// The debug clip shows the AI cycle from a wide view with the menu closed. The player first
/// holds fire until an enemy has spent its ammunition on it and runs for shelter (Recover),
/// because one that is shot at from the start dies long before. Then it shoots back, and
/// the clip shows the flight, the reload out of sight and the return.
void dotio_debug_clip(Stage &stage) {
    stage.start_dotio();
    dotio_debug_view(stage, dotio_debug_clip_zoom_out);
    stage.press(Key::backquote);
    const int limit = dotio_recovery_wait_seconds * clip_fps;
    const float margin = stage.viewport().y * dotio_recovery_margin;
    const int waited = stage.play_until(limit, dotio_sidestep, [&] {
        return dotio_recovery_in_view(stage.dotio(), stage.runtime().camera, margin);
    });
    if (waited == limit)
        std::cerr << "kinetra_capture: no Dotio enemy recovered within " << dotio_recovery_wait_seconds
                  << " s; dotio-debug-clip shows none at its start\n";
    stage.record(long_clip_frames, [&](int frame) {
        return dotio_shoot(stage, waited + frame, dotio_sidestep(waited + frame));
    });
}

// i4sim: the captured blueprint (i4sim_capture_blueprint) with the Reciprocal profile and
// plain A*, running at the session's own rate; Debug Mode is on at the start.

/// What an i4sim capture shows around the floor.
enum class I4simView {
    /// Debug Mode off: the floor, the labels and the statistics.
    normal,
    /// Debug Mode with every layer, the debug menu closed.
    debug,
    /// Debug Mode with the debug menu open on LAYERS: every part and its swatch.
    debug_with_layers,
};

/// Loads the captured blueprint, chooses its motion settings and runs it through the
/// warm-up. The settings are rows of the debug menu, which only Debug Mode shows, so the
/// normal view turns Debug Mode off after them.
void i4sim_start(Stage &stage, I4simView view) {
    stage.start_i4sim(i4sim_capture_blueprint);
    stage.press(Key::backquote);
    for (const auto &click : i4sim_motion_clicks)
        stage.click_debug_row(click.row);
    if (view == I4simView::debug_with_layers)
        stage.click_debug_row(i4sim_layers_click.row);
    else
        stage.press(Key::backquote);
    stage.point_at(stage.viewport() - Vec2{2, 2});
    if (view == I4simView::normal)
        stage.toggle_setting(SettingsRow::debug_mode);
    stage.press(Key::space);
    stage.wait(i4sim_warm_up_seconds);
}

void i4sim(Stage &stage) {
    i4sim_start(stage, I4simView::normal);
    stage.shot();
}

void i4sim_debug(Stage &stage) {
    // The LAYERS section fits the screenshot's height and leaves most of the floor free.
    i4sim_start(stage, I4simView::debug_with_layers);
    stage.shot();
}

Input no_input(int) {
    return {};
}

void i4sim_clip(Stage &stage) {
    i4sim_start(stage, I4simView::normal);
    stage.record(long_clip_frames, no_input);
}

void i4sim_debug_clip(Stage &stage) {
    // The menu stays closed: at clip size it would cover a fifth of the floor.
    i4sim_start(stage, I4simView::debug);
    stage.record(long_clip_frames, no_input);
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
