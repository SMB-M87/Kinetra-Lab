#include "capture/scenes/scenes.h"

#include "capture/dotio_player.h"
#include "capture/scenes/busy_stretch.h"
#include "capture/scenes/showcase.h"
#include "capture/scenes/ui_clips.h"
#include "capture/stage.h"

#include "demo/application.h"
#include "demo/scenario.h"
#include "i4sim/blueprint/blueprint.h"

#include <kinetra/runtime/launcher.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace capture {
std::filesystem::path output_subdirectory(Gallery gallery) {
    return gallery == Gallery::kinetra ? "kinetra" : "";
}

void apply_menu_click(std::span<const kinetra::DebugEntry> menu, const MenuClick &click,
                      std::string_view application) {
    const bool found = click.row < menu.size() && menu[click.row].label == click.label;
    if (!found || !menu[click.row].action)
        throw std::runtime_error(
            "The " + std::string(application) + " debug menu changed: row " + std::to_string(click.row) +
            " is " + (click.row < menu.size() ? "'" + menu[click.row].label + "'" : "missing") +
            (found ? ", which has no action" : ", not '" + std::string(click.label) + "'"));
    menu[click.row].action();
}

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
/// Lengths of the Demo scenario clips from their start: Gravity until its shapes rest (8 s),
/// Maze until the player has walked the route and stands at its goal (26 s). A scenario whose
/// groups turn together lasts its first leg and the pause, until the groups turn back, so
/// the GIF loops as the scenario repeats: Doorway 8 s, Corridor 13.6 s, Crossing 11 s, Circle
/// 9.6 s, Bottleneck 13.6 s. Chaos, Fleet and Swarm run 12 s: Fleet's formations meet once
/// and Swarm's lure crosses its pillar.
constexpr int demo_gravity_frames = 120, demo_maze_frames = 390, demo_doorway_frames = 120,
              demo_corridor_frames = 204, demo_crossing_frames = 165, demo_circle_frames = 144,
              demo_bottleneck_frames = 204, demo_free_frames = 180;
/// Length of the Dotio debug clip and the i4sim clips: twelve seconds. Long enough for a
/// Dotio enemy to run for shelter, reload and come back, and for an i4sim mover to cross
/// its floor at the session's own speed.
constexpr int long_clip_frames = 180;
/// Clip frames per second.
constexpr int clip_fps = Stage::clip_fps;
/// Seconds a session runs before it is captured, so agents and enemies are under way. Dotio's
/// enemies first have to find the player.
constexpr double warm_up_seconds = 2, dotio_warm_up_seconds = 8;
/// Steps of session time per second.
constexpr int steps_per_second = clip_fps * Stage::steps_per_frame;
/// Steps a long clip records.
constexpr int long_clip_steps = long_clip_frames * Stage::steps_per_frame;
/// Seconds of the captured i4sim floor that are searched for its busiest stretch. The floor
/// needs about a minute before most movers have work.
constexpr int i4sim_search_seconds = 150;
/// Wheel notches the Dotio debug scenes zoom out: the screenshot for a wider view of the
/// agents, the clip far enough to show the whole area the enemies spawn in.
constexpr float dotio_debug_zoom_out = -3, dotio_debug_clip_zoom_out = -9;
/// Longest a Dotio screenshot waits for an enemy in sight with a bolt on its way, in seconds.
constexpr int dotio_fight_wait_seconds = 20;
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

void launcher_menu(Stage &stage, std::optional<Variant> configuration_page = std::nullopt) {
    kinetra::LaunchMenu menu(launcher_choices());
    Input input;
    input.mouse = stage.viewport() - Vec2{2, 2};
    menu.update(input, stage.viewport());
    if (configuration_page == Variant::dotio) {
        Input down = input;
        down.pressed[std::size_t(Key::down)] = true;
        menu.update(down, stage.viewport());
    }
    if (configuration_page) {
        input.pressed[std::size_t(Key::enter)] = true;
        menu.update(input, stage.viewport());
    }
    menu.draw(stage.runtime());
    stage.write_frame();
}

void launcher_page(Stage &stage) {
    launcher_menu(stage);
}

void launcher_demo_scenarios(Stage &stage) {
    launcher_menu(stage, Variant::demo);
}

void launcher_difficulty(Stage &stage) {
    launcher_menu(stage, Variant::dotio);
}

// Demo: starts in Debug Mode with its default layers on; the normal view turns it off.

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
    stage.wait(warm_up_seconds);
}

void demo(Stage &stage) {
    demo_start(stage, false);
    stage.shot();
}

void demo_debug(Stage &stage) {
    demo_start(stage, true);
    // The debug menu with its MOTION section: the values that can be tuned while it runs.
    stage.toggle_debug_menu();
    stage.click_debug_row(demo_motion_click.row);
    stage.point_at(stage.viewport() - Vec2{2, 2});
    stage.shot();
}

/// Where the Maze clip clicks, in world units: a cell across the labyrinth from the player,
/// so the route winds through it.
constexpr Vec2 demo_maze_goal{16, 8};

/// Records `frames` of Demo's `scenario` from its start, in Debug Mode as it starts. The
/// Maze clip clicks demo_maze_goal in its first frame, so the player walks the route, and
/// throws std::runtime_error when the click set no goal or the clip ended before the player
/// arrived: a missed click or a slower walk would otherwise leave a clip that stops halfway.
void demo_scenario_clip(Stage &stage, demo::Scenario scenario, int frames) {
    stage.start(Variant::demo, static_cast<std::size_t>(scenario));
    stage.step();
    const bool maze = scenario == demo::Scenario::maze;
    stage.record(frames, [&](int frame) {
        Input input;
        if (maze && frame == 0) {
            stage.aim(stage.runtime().camera.world_to_screen(demo_maze_goal));
            input.clicked = input.mouse_down = true;
        }
        if (maze && frame == 1 && !stage.demo().scene().player_goal())
            throw std::runtime_error("The Maze clip's click gave the player no goal");
        return input;
    });
    if (maze && stage.demo().scene().player_goal())
        throw std::runtime_error("The Maze clip ended before the player reached its goal");
}

void demo_clip(Stage &stage) {
    demo_start(stage, false);
    stage.record(clip_frames, [&](int frame) { return demo_walk(stage, frame); });
}

void demo_debug_clip(Stage &stage) {
    demo_start(stage, true);
    stage.record(clip_frames, [&](int frame) { return demo_walk(stage, frame); });
}

// Dotio: debug is opt-in; the debug view turns on Debug Mode and every visual row. The player
// is scripted against the world of the running session (dotio_player.h) instead of along a
// fixed timeline, so it reacts to the enemies it meets without knowing their moves ahead.

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

/// Turns on Debug Mode with Shift+Ctrl and clicks dotio_debug_clicks: INVULNERABLE and every
/// layer of the debug menu. Then zooms `notches` wheel notches (negative zooms out). Leaves
/// the menu open on its LAYERS section and the pointer in the field.
void dotio_debug_view(Stage &stage, float notches) {
    stage.toggle_debug_menu();
    for (const auto &click : dotio_debug_clicks)
        stage.click_debug_row(click.row);
    Input zoom;
    zoom.wheel = notches;
    stage.point_at(at(stage, .7f, .5f));
    stage.step(zoom);
}

/// Frames the Dotio warm-up plays; a scene that plays on continues the count from here.
constexpr int dotio_warm_up_frames = static_cast<int>(dotio_warm_up_seconds * clip_fps);

void dotio_start(Stage &stage, bool debug) {
    stage.start_dotio();
    if (debug)
        dotio_debug_view(stage, dotio_debug_zoom_out);
    stage.point_at(at(stage, .7f, .45f));
    stage.play(dotio_warm_up_frames, [&](int frame) { return dotio_play(stage, frame); });
}

/// Whether the player has an enemy in sight and a bolt on its way to it.
bool dotio_fight_in_view(Stage &stage) {
    if (!dotio_target(stage.dotio(), stage.runtime().camera))
        return false;
    for (auto &bolt : stage.dotio().world().get_bolts())
        if (bolt.is_active)
            return true;
    return false;
}

/// Takes the screenshot of a started Dotio scene at a moment that shows the game: the player
/// plays on until it shoots at an enemy in sight. The pointer then leaves the spot the
/// player aimed at, because over the open debug menu it would highlight a row.
void dotio_shot(Stage &stage) {
    stage.play_until(
        dotio_fight_wait_seconds * clip_fps,
        [&](int frame) { return dotio_play(stage, dotio_warm_up_frames + frame); },
        [&] { return dotio_fight_in_view(stage); });
    stage.point_at(stage.viewport() - Vec2{2, 2});
    stage.shot();
}

void dotio(Stage &stage) {
    dotio_start(stage, false);
    dotio_shot(stage);
}

void dotio_debug(Stage &stage) {
    dotio_start(stage, true);
    dotio_shot(stage);
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
    stage.record(clip_frames, [&](int frame) { return dotio_play(stage, dotio_warm_up_frames + frame); });
}

/// The debug clip shows the AI cycle from a wide view with the menu closed. The player first
/// holds fire until an enemy has spent its ammunition on it and runs for shelter (Recover),
/// because one that is shot at from the start dies long before. Then it shoots back, and
/// the clip shows the flight, the reload out of sight and the return.
void dotio_debug_clip(Stage &stage) {
    stage.start_dotio();
    dotio_debug_view(stage, dotio_debug_clip_zoom_out);
    stage.toggle_debug_menu();
    const int limit = dotio_recovery_wait_seconds * clip_fps;
    const float margin = stage.viewport().y * dotio_recovery_margin;
    const auto recovering = [&] {
        return dotio_recovery_in_view(stage.dotio(), stage.runtime().camera, margin);
    };
    const int waited = stage.play_until(limit, dotio_sidestep, recovering);
    if (!recovering())
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

/// The step after Space at which the busiest stretch of a long clip begins on the captured
/// floor, on a whole second: the twelve seconds in which most movers drive. Measured in a
/// headless run of the same floor (busy_stretch.h), so it follows the simulation.
int i4sim_busy_start() {
    const auto driving = driving_movers(i4sim::default_blueprint_directory() / i4sim_capture_blueprint,
                                        i4sim_search_seconds * steps_per_second);
    return busiest_window_start(driving, long_clip_steps, steps_per_second);
}

/// Loads the captured blueprint, chooses its motion settings and runs it to `into` steps
/// into its busiest stretch. The settings are rows of the debug menu, which only Debug Mode
/// shows, so the normal view turns Debug Mode off after them.
void i4sim_start(Stage &stage, I4simView view, int into) {
    stage.start_i4sim(i4sim_capture_blueprint);
    stage.toggle_debug_menu();
    for (const auto &click : i4sim_motion_clicks)
        stage.click_debug_row(click.row);
    if (view == I4simView::debug_with_layers)
        stage.click_debug_row(i4sim_layers_click.row);
    else
        stage.toggle_debug_menu();
    stage.point_at(stage.viewport() - Vec2{2, 2});
    if (view == I4simView::normal)
        stage.toggle_setting(SettingsRow::debug_mode);
    stage.press(Key::space);
    stage.steps(i4sim_busy_start() + into);
}

/// A screenshot shows the middle of the stretch the clips record.
constexpr int i4sim_shot_steps = long_clip_steps / 2;

void i4sim(Stage &stage) {
    i4sim_start(stage, I4simView::normal, i4sim_shot_steps);
    stage.shot();
}

void i4sim_debug(Stage &stage) {
    // The LAYERS section fits the screenshot's height and leaves most of the floor free.
    i4sim_start(stage, I4simView::debug_with_layers, i4sim_shot_steps);
    stage.shot();
}

Input no_input(int) {
    return {};
}

void i4sim_clip(Stage &stage) {
    i4sim_start(stage, I4simView::normal, 0);
    stage.record(long_clip_frames, no_input);
}

void i4sim_debug_clip(Stage &stage) {
    // The menu stays closed: at clip size it would cover a fifth of the floor.
    i4sim_start(stage, I4simView::debug, 0);
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

/// A clip of the shared UI for Kinetra's README, at the size of the other clips.
Scene ui_clip(std::string_view name, void (*script)(Stage &)) {
    return {name, SceneKind::clip, clip_width, clip_height, script, Gallery::kinetra};
}
} // namespace

const std::vector<Scene> &scenes() {
    static const std::vector<Scene> all{
        shot("launcher", launcher_page),
        shot("launcher-demo-scenarios", launcher_demo_scenarios),
        shot("launcher-difficulty", launcher_difficulty),
        shot("demo", demo),
        shot("demo-debug", demo_debug),
        clip("demo-clip", demo_clip),
        clip("demo-debug-clip", demo_debug_clip),
        clip("demo-gravity-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::gravity, demo_gravity_frames); }),
        clip("demo-maze-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::maze, demo_maze_frames); }),
        clip("demo-doorway-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::doorway, demo_doorway_frames); }),
        clip("demo-corridor-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::corridor, demo_corridor_frames); }),
        clip("demo-crossing-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::crossing, demo_crossing_frames); }),
        clip("demo-circle-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::circle, demo_circle_frames); }),
        clip("demo-bottleneck-clip",
             [](Stage &stage) {
                 demo_scenario_clip(stage, demo::Scenario::bottleneck, demo_bottleneck_frames);
             }),
        clip("demo-chaos-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::chaos, demo_free_frames); }),
        clip("demo-fleet-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::fleet, demo_free_frames); }),
        clip("demo-swarm-clip",
             [](Stage &stage) { demo_scenario_clip(stage, demo::Scenario::swarm, demo_free_frames); }),
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
        ui_clip("ui-launcher", ui_launcher),
        ui_clip("ui-settings", ui_settings),
        ui_clip("ui-debug-menu", ui_debug_menu),
        ui_clip("ui-inspector", ui_inspector),
        ui_clip("ui-debug-layers", ui_debug_layers),
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
