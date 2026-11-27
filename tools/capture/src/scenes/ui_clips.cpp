#include "capture/scenes/ui_clips.h"

#include "capture/stage.h"
#include "capture/tour/clip_recorder.h"
#include "capture/tour/tour.h"

#include "demo/application.h"
#include "demo/presentation/session.h"
#include "demo/scenario.h"
#include "demo/world/inspector.h"

#include "kinetra_lab/inspector.h"
#include <kinetra/runtime/inspector_view.h>
#include <kinetra/runtime/launcher.h>
#include <kinetra/runtime/settings_view.h>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

namespace capture {
namespace {
using kinetra::SettingsOverlay;
using kinetra::Vec2;

/// Rows of the launcher's menu: DEMO, DOTIO and I4SIM.
constexpr std::size_t launcher_rows = 3;
/// Help lines of Demo's SIM CONTROLS and DEBUG CONTROLS sections, the latter with the two
/// lines every application shares; they place the rows of an open settings panel.
constexpr std::size_t demo_sim_lines = 4, demo_debug_lines = 3;
/// Tour frames per clip frame: the tour plays at 60 frames per second, the clips at 15.
constexpr int keep_every = tour_frames_per_second / Stage::clip_fps;
static_assert(keep_every * Stage::clip_fps == tour_frames_per_second);
/// Where a clip's pointer rests when nothing is clicked, as a fraction of the window: low on
/// the right, away from the menus and the inspector.
constexpr float rest_x = .8f, rest_y = .78f;
/// Distance from the inspector's top-right corner to the middle of its X button, in logical
/// pixels: the button's margin and half its side (Kinetra's inspector_view.cpp).
constexpr float inspector_close_inset = 12;
/// The layers the layers clip toggles and toggles back, in the menu's order: VECTORS and PATHS
/// start on, the others off. HOME RANGE, which starts off too, comes after them.
constexpr std::string_view demo_layers[]{"VECTORS", "STEERING PROBES", "PATHS", "COLLISION SHAPES",
                                         "ACTIVE AGENTS"};

/// A point at a fraction of the window.
Vec2 at(const Tour &tour, float x, float y) {
    return {tour.viewport().x * x, tour.viewport().y * y};
}

/// Plays `script` in a Tour on the stage's window and records it as the stage's clip, from
/// the moment the script calls ClipRecorder::start().
void record_ui_clip(Stage &stage, const std::function<void(Tour &, ClipRecorder &)> &script) {
    ClipRecorder recorder(stage.clip_directory(), keep_every, Stage::clip_fps);
    Tour tour(stage.runtime(), [&](const Canvas &frame) { recorder.record(frame); });
    script(tour, recorder);
    if (recorder.written() < 2)
        throw std::runtime_error("The clip " + stage.clip_directory().filename().string() +
                                 " recorded fewer than two frames");
}

/// Goes through the launcher to Demo's `scenario`, as a person would: DEMO, then the
/// scenario on the configuration page. Throws std::runtime_error when a menu page, another
/// application or another scenario runs instead.
void start_demo(Tour &tour, demo::Scenario scenario) {
    tour.click_menu_page_row(0, launcher_rows);
    tour.click_menu_page_row(static_cast<std::size_t>(scenario), demo::scenario_labels().size());
    tour.wait(0.2);
    const auto *running = tour.in_menu() ? nullptr : dynamic_cast<demo::Session *>(&tour.session());
    if (!running || running->application().scene().scenario() != scenario)
        throw std::runtime_error("The clip did not start Demo " +
                                 std::string(demo::scenario_label(scenario)));
}

/// Throws std::runtime_error with `what` unless Demo has an agent selected (`selected`) or
/// none: a click that missed would otherwise leave a GIF that shows something else.
void require_selection(Tour &tour, bool selected, std::string_view what) {
    if (bool(demo_application(tour).scene().selected_agent()) != selected)
        throw std::runtime_error("The inspector clip " + std::string(what));
}

/// Moves the pointer to where it rests between actions.
void rest(Tour &tour) {
    tour.move_to(at(tour, rest_x, rest_y));
}

/// Screen position of the X button of Demo's inspector while it shows the selected agent,
/// with the debug menu hidden and the frame-time graph shown or not.
Vec2 inspector_close_button(Tour &tour, bool frame_times) {
    const auto snapshot = demo::inspect_agent(demo_application(tour).scene());
    if (!snapshot)
        throw std::runtime_error("The inspector clip has no selected agent to close");
    const auto area = kinetra_lab::inspector_area(kinetra::DebugMenu{}, tour.viewport(), frame_times);
    const auto bounds = kinetra::InspectorView{}.bounds(*snapshot, area);
    return {bounds.max.x - inspector_close_inset, bounds.min.y + inspector_close_inset};
}

/// Clicks every row of `labels` in turn.
void click_rows(Tour &tour, std::initializer_list<std::string_view> labels) {
    for (const auto label : labels) {
        tour.click_row(label);
        tour.pause(0.1, 0.2);
    }
}
} // namespace

void ui_launcher(Stage &stage) {
    record_ui_clip(stage, [](Tour &tour, ClipRecorder &recorder) {
        recorder.start();
        tour.wait(0.5);
        // Hover preselects an entry without starting it.
        const auto menu = tour.menu_layout(launcher_rows);
        for (const std::size_t row : {std::size_t{2}, std::size_t{1}, std::size_t{0}}) {
            tour.move_to(menu.row_center(row) + Vec2{40, 3});
            tour.pause(0.3, 0.45);
        }
        tour.click();
        tour.wait(0.5);
        const auto scenarios = tour.menu_layout(demo::scenario_labels().size());
        for (const auto scenario : {demo::Scenario::gravity, demo::Scenario::maze}) {
            tour.move_to(scenarios.row_center(static_cast<std::size_t>(scenario)) + Vec2{40, 3});
            tour.pause(0.3, 0.45);
        }
        tour.click_menu_page_row(static_cast<std::size_t>(demo::Scenario::doorway),
                                 demo::scenario_labels().size());
        tour.wait(1.6);
        // SCENARIOS in the debug menu returns to the configuration page, DOORWAY selected.
        tour.toggle_debug_menu();
        tour.pause(0.4, 0.6);
        tour.click_row("SCENARIOS");
        const auto &page = tour.menu();
        if (!tour.in_menu() || !page.configuring() ||
            page.row_name(page.selected()) != demo::scenario_label(demo::Scenario::doorway))
            throw std::runtime_error("SCENARIOS did not return to Demo's page with DOORWAY selected");
        tour.wait(1.2);
    });
}

void ui_settings(Stage &stage) {
    record_ui_clip(stage, [](Tour &tour, ClipRecorder &recorder) {
        start_demo(tour, demo::Scenario::doorway);
        rest(tour);
        recorder.start();
        tour.wait(0.6);
        tour.open_settings(demo_sim_lines, demo_debug_lines);
        tour.pause(0.4, 0.6);
        tour.click_setting(SettingsRow::statistics);
        tour.wait(0.6);
        tour.click_setting(SettingsRow::statistics);
        tour.pause();
        for (const auto section : {SettingsRow::sim_controls, SettingsRow::debug_controls}) {
            tour.click_setting(section);
            tour.wait(1.1);
            tour.click_setting(section);
            tour.pause();
        }
        tour.click_settings_action(SettingsOverlay::resume_x);
        tour.wait(0.8);
    });
}

void ui_debug_menu(Stage &stage) {
    record_ui_clip(stage, [](Tour &tour, ClipRecorder &recorder) {
        start_demo(tour, demo::Scenario::doorway);
        rest(tour);
        recorder.start();
        tour.wait(0.5);
        tour.toggle_debug_menu();
        tour.pause(0.4, 0.6);
        tour.step_row("SPEED", true);
        tour.wait(0.8);
        tour.click_row("MOTION");
        tour.pause();
        tour.step_row("MAX SPEED", true, 3);
        tour.wait(0.6);
        click_rows(tour, {"AVOIDANCE", "LEGACY"});
        tour.wait(1.0);
        click_rows(tour, {"AVOIDANCE", "RECIPROCAL"});
        tour.click_row("LAYERS");
        tour.wait(0.8);
        rest(tour);
        tour.toggle_debug_menu();
        tour.wait(0.8);
    });
}

void ui_inspector(Stage &stage) {
    record_ui_clip(stage, [](Tour &tour, ClipRecorder &recorder) {
        start_demo(tour, demo::Scenario::doorway);
        tour.wait(0.6);
        rest(tour);
        recorder.start();
        tour.wait(0.4);
        tour.click_on([&] { return nearest_demo_agent(tour, at(tour, .4f, .45f)); });
        require_selection(tour, true, "selected no agent");
        tour.wait(1.4);
        tour.open_settings(demo_sim_lines, demo_debug_lines);
        tour.pause();
        tour.click_setting(SettingsRow::frame_times);
        tour.pause();
        tour.click_settings_action(SettingsOverlay::resume_x);
        tour.wait(1.4);
        tour.click_on([&] { return nearest_demo_agent(tour, at(tour, .62f, .55f)); });
        tour.wait(1.6);
        tour.click_at(inspector_close_button(tour, true));
        require_selection(tour, false, "did not close the inspector with its X");
        tour.wait(0.8);
    });
}

void ui_debug_layers(Stage &stage) {
    record_ui_clip(stage, [](Tour &tour, ClipRecorder &recorder) {
        start_demo(tour, demo::Scenario::terrain);
        tour.toggle_debug_menu();
        // Agents that wander by planned routes, so PATHS has routes to draw. The player stays
        // where it is, so the camera does not move and the GIF stores little per frame.
        click_rows(tour, {"AGENTS", "WANDER", "ROUTE", "LAYERS"});
        tour.wait(1.5);
        recorder.start();
        tour.wait(0.3);
        for (const auto layer : demo_layers) {
            tour.click_row(layer);
            tour.pause(0.15, 0.25);
        }
        tour.wait(0.4);
        for (const auto layer : demo_layers) {
            tour.click_row(layer);
            tour.pause(0.1, 0.15);
        }
        tour.click_row("HOME RANGE");
        tour.wait(0.6);
        // Zooming out shows the streamed terrain around the player; the GIF's loop zooms back.
        rest(tour);
        tour.scroll(-4);
        tour.wait(0.6);
    });
}
} // namespace capture
