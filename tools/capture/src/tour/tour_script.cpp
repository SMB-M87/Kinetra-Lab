// What the tour shows, in order: the launcher, Demo, Dotio, i4sim and EXIT. Every chapter is
// written in the verbs of Tour, with a caption for each thing a viewer should notice.
// Captions use only the characters of the engine's bitmap font: letters, digits and
// `/ + [ ] : - _ . , *`.
#include "capture/dotio_player.h"
#include "capture/tour/tour.h"

#include "demo/application.h"
#include "demo/presentation/session.h"
#include "dotio/presentation/session.h"
#include "i4sim/blueprint/blueprint.h"
#include "i4sim/simulation/view.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace capture {
namespace {
using kinetra::Key;
using kinetra::SettingsOverlay;
using kinetra::Vec2;

/// Help lines of each application's SIM CONTROLS and DEBUG CONTROLS sections (the latter
/// with the two lines every application shares), which place the rows of an open panel.
constexpr std::size_t demo_sim_lines = 4, demo_debug_lines = 3, dotio_sim_lines = 3, dotio_debug_lines = 3,
                      i4sim_sim_lines = 5, i4sim_debug_lines = 5;
/// Rows of the launcher's menu and of Dotio's difficulty page.
constexpr std::size_t launcher_rows = 3, difficulty_rows = 3;
/// The showcase scene and the floor the tour loads, as file names in the catalog.
constexpr std::string_view showcase_scene = "h-crossing.json", floor_blueprint = "4320x2400.json";

/// A point at a fraction of the window.
Vec2 at(const Tour &tour, float x, float y) {
    return {tour.viewport().x * x, tour.viewport().y * y};
}

/// Clicks the row `label` again `times` times where the pointer already is.
void click_again(Tour &tour, int times) {
    for (int i = 0; i < times; ++i) {
        tour.wait(0.28);
        tour.click();
    }
}

/// Clicks every row of `labels` in turn, as someone who goes down a list.
void click_rows(Tour &tour, std::initializer_list<std::string_view> labels) {
    for (const auto label : labels) {
        tour.click_row(label);
        tour.pause(0.12, 0.26);
    }
}

// The launcher.

void launcher_chapter(Tour &tour) {
    tour.title("KINETRA-LAB", 2.4);
    tour.fade_in(0.7);
    tour.wait(2.2);
    tour.caption("THREE APPLICATIONS ON THE KINETRA ENGINE");
    const auto layout = tour.menu_layout(launcher_rows);
    for (const std::size_t row : {std::size_t{2}, std::size_t{1}, std::size_t{0}}) {
        tour.move_to(layout.row_center(row) + Vec2{60, 4});
        tour.pause(0.35, 0.6);
    }
    tour.caption("DEMO: TERRAIN, NAVIGATION, STEERING AND PHYSICS");
    tour.pause(0.6, 0.9);
    tour.click();
    tour.caption("CHOOSE A DEMO SCENARIO");
    tour.move_to(tour.menu_layout(demo::scenario_labels().size()).row_center(0));
    tour.pause(0.35, 0.6);
    tour.click();
}

// Demo.

void demo_chapter(Tour &tour) {
    tour.wait(1.4);
    tour.caption("CTRL OPENS THE SETTINGS PANEL");
    tour.open_settings(demo_sim_lines, demo_debug_lines);
    tour.pause(0.7, 1.0);
    tour.click_setting(SettingsRow::sim_controls);
    tour.wait(1.6);
    tour.click_setting(SettingsRow::sim_controls);
    tour.caption("DEBUG MODE OFF: THE PLAIN SCENE");
    tour.click_setting(SettingsRow::debug_mode);
    tour.pause(0.4, 0.6);
    tour.click_settings_action(SettingsOverlay::resume_x);
    tour.wait(1.6);

    tour.caption("W A S D MOVES THE PLAYER, Q AND E TURN IT");
    tour.hold({Key::d}, 1.1);
    tour.hold({Key::w}, 0.8);
    tour.hold({Key::q}, 0.6);
    tour.hold({Key::a}, 0.9);
    tour.hold({Key::e}, 0.6);
    tour.hold({Key::s}, 0.7);

    tour.caption("SHIFT + CTRL SWITCHES DEBUG MODE ON AND OPENS THE MENU");
    tour.toggle_debug_menu();
    tour.wait(1.8);
    tour.caption("A CLICK PLANS A ROUTE AND THE PLAYER WALKS IT");
    tour.click_at(at(tour, .70f, .34f));
    tour.wait(2.6);

    tour.caption("LAYERS: WHAT DEBUG MODE DRAWS, EACH IN ITS COLOR");
    tour.click_row("LAYERS");
    tour.pause();
    // STEERING PROBES, COLLISION SHAPES and ACTIVE AGENTS start off: all five layers on for a
    // look, then three off, so HOME RANGE shows on PATHS and ACTIVE AGENTS, as before.
    click_rows(tour, {"STEERING PROBES", "COLLISION SHAPES", "ACTIVE AGENTS"});
    tour.wait(0.8);
    click_rows(tour, {"STEERING PROBES", "VECTORS", "COLLISION SHAPES"});
    tour.wait(0.8);
    tour.caption("HOME RANGE: AGENTS STAY AROUND THE PLACE THEY STARTED");
    tour.click_row("HOME RANGE");
    tour.wait(1.2);
    tour.click_on([&] { return nearest_demo_agent(tour, at(tour, .58f, .52f)); });
    tour.wait(3.0);

    tour.caption("AGENTS: THE HOME RANGE CHANGES WHILE THEY WALK");
    tour.click_row("AGENTS");
    tour.pause();
    tour.step_row("HOME RANGE", false, 4);
    tour.wait(4.0);
    tour.step_row("HOME RANGE", true, 5);
    tour.wait(2.0);
    tour.caption("WANDER BY PLANNED ROUTES INSTEAD OF BY STEERING");
    tour.click_row("WANDER");
    tour.click_row("ROUTE");
    tour.wait(4.2);
    tour.click_row("WANDER");
    tour.click_row("STEERING");
    tour.wait(1.2);

    tour.caption("MOTION: TOP SPEED, CHANGED WHILE THE SCENE RUNS");
    tour.click_row("MOTION");
    tour.pause();
    tour.step_row("MAX SPEED", true, 3);
    tour.wait(3.6);
    tour.caption("LEGACY STEERING AGAINST RECIPROCAL AVOIDANCE");
    tour.click_row("AVOIDANCE");
    tour.click_row("LEGACY");
    tour.wait(3.6);
    tour.click_row("AVOIDANCE");
    tour.click_row("RECIPROCAL");
    tour.wait(1.0);

    tour.caption("SIMULATION: SLOW MOTION, PAUSE AND SINGLE TICKS");
    tour.click_row("SIMULATION");
    tour.pause();
    tour.step_row("SPEED", false, 2);
    tour.wait(3.4);
    tour.step_row("SPEED", true, 2);
    tour.click_row("RUNNING");
    tour.wait(0.9);
    tour.click_row("SINGLE TICK");
    click_again(tour, 5);
    tour.wait(0.5);
    tour.click_row("RUNNING");
    tour.caption("GRAVITY ON, AND OFF AGAIN");
    tour.click_row("GRAVITY");
    tour.wait(1.7);
    tour.click_row("GRAVITY");
    tour.wait(0.8);
    tour.caption("RESET PARAMETERS RESTORES EVERY DEFAULT");
    tour.click_row("RESET PARAMETERS");
    tour.wait(1.2);

    // Commanded agents keep their goal for twenty seconds, so this comes last: the zoomed-out
    // view below shows the whole crowd on its way, and nothing after it needs a calm scene.
    tour.caption("CLICK SETS GOAL: ONE CLICK SENDS EVERY AGENT THERE");
    tour.click_row("AGENTS");
    tour.pause();
    tour.click_row("CLICK SETS GOAL");
    tour.click_at(at(tour, .62f, .64f));
    tour.wait(3.5);
    tour.click_row("CLICK SETS GOAL");

    tour.caption("ZOOM IS A DEBUG TOOL: ITS ROW, AND THE WHEEL");
    tour.step_row("ZOOM", false, 5);
    tour.wait(0.8);
    tour.move_to(at(tour, .60f, .50f));
    tour.scroll(-4);
    tour.wait(1.2);
    tour.caption("GHOST CAMERA: THE VIEW LEAVES THE PLAYER");
    tour.click_row("GHOST CAMERA");
    tour.toggle_debug_menu();
    tour.hold({Key::right}, 1.5);
    tour.hold({Key::up}, 1.0);
    tour.caption("A CLICK IN THE GHOST VIEW TELEPORTS THE PLAYER");
    // A blocked teleport does nothing, so try a few places until one takes the player.
    for (const Vec2 fraction : {Vec2{.50f, .50f}, Vec2{.42f, .44f}, Vec2{.58f, .58f}, Vec2{.36f, .62f}}) {
        tour.click_at(at(tour, fraction.x, fraction.y));
        tour.wait(0.5);
        if (demo_application(tour).last_action() == demo::ActionResult::teleported)
            break;
    }
    tour.wait(1.2);
    tour.toggle_debug_menu();
    tour.click_row("GHOST CAMERA");
    tour.step_row("ZOOM", true, 9);
    tour.toggle_debug_menu();
    tour.wait(0.8);

    tour.caption("SETTINGS: STATISTICS, FRAME TIMES AND THE FRAME LIMIT");
    tour.open_settings(demo_sim_lines, demo_debug_lines);
    tour.pause(0.5, 0.8);
    tour.click_setting(SettingsRow::statistics);
    tour.pause(0.5, 0.8);
    tour.click_setting(SettingsRow::statistics);
    tour.click_setting(SettingsRow::debug_controls);
    tour.wait(1.8);
    tour.click_setting(SettingsRow::debug_controls);
    tour.caption("SCENARIOS RETURNS TO THE SCENARIO PAGE, LAUNCHER TO THE MENU");
    tour.pause(0.5, 0.8);
    tour.click_settings_action(SettingsOverlay::leave_x);
    tour.wait(0.9);
    tour.click_menu_footer(kinetra::FooterButton::launcher, demo::scenario_labels().size());
    tour.wait(0.9);
}

// Dotio. The player is steered by what the world shows, so it reacts to the enemies it meets.

dotio::Application &dotio_application(Tour &tour) {
    return dynamic_cast<dotio::Session &>(tour.session()).application();
}

/// The four directions the player moves in, in turn.
constexpr Key dotio_route[]{Key::d, Key::s, Key::a, Key::w};

/// Plays `seconds` of Dotio: the pointer tracks the enemy the scripted player would shoot
/// and fires in bursts; with `roam` the player also walks a square through the corridors,
/// one second per side. Leaves keys and button released.
void dotio_fight(Tour &tour, double seconds, bool roam) {
    tour.play(seconds, [&](int frame) {
        if (roam)
            tour.set_held({dotio_route[(frame / tour_frames_per_second) % 4]});
        if (const auto aim = dotio_aim(dotio_application(tour), tour.camera())) {
            tour.aim_at(*aim);
            // Bursts leave mana for the next enemy and show the bolts in flight.
            tour.press_mouse(frame % tour_frames_per_second < 34);
        } else
            tour.press_mouse(false);
    });
    tour.set_held({});
    tour.press_mouse(false);
    tour.frame();
}

/// A free place for the player on screen, found by trying points around the middle of the
/// window; the middle itself when none is free.
Vec2 free_teleport_point(Tour &tour) {
    auto &application = dotio_application(tour);
    const float radius = application.world().get_player().radius;
    for (const Vec2 fraction : {Vec2{.56f, .46f}, Vec2{.44f, .56f}, Vec2{.62f, .60f}, Vec2{.38f, .40f},
                                Vec2{.50f, .34f}, Vec2{.50f, .66f}, Vec2{.68f, .44f}, Vec2{.32f, .56f}}) {
        const Vec2 point = at(tour, fraction.x, fraction.y);
        if (application.teleport_clear(dotio::from_world(tour.camera().screen_to_world(point)), radius))
            return point;
    }
    return at(tour, .5f, .5f);
}

void dotio_chapter(Tour &tour) {
    tour.caption("DOTIO: A SURVIVAL GAME AND AI EXPERIMENT");
    tour.click_menu_page_row(1, launcher_rows);
    tour.pause(0.6, 0.9);
    const auto difficulties = tour.menu_layout(difficulty_rows);
    tour.move_to(difficulties.row_center(0) + Vec2{50, 2});
    tour.pause(0.3, 0.5);
    tour.move_to(difficulties.row_center(2) + Vec2{50, 2});
    tour.pause(0.3, 0.5);
    tour.click_menu_page_row(1, difficulty_rows);
    tour.wait(1.0);

    tour.caption("W A S D MOVES, THE MOUSE AIMS, HOLD THE BUTTON TO SHOOT");
    dotio_fight(tour, 12, true);

    tour.caption("SHIFT + CTRL SWITCHES DEBUG MODE ON AND OPENS THE MENU");
    tour.toggle_debug_menu();
    tour.pause(0.6, 0.9);
    // The tour has to survive its own demonstration.
    tour.click_row("INVULNERABLE");
    tour.caption("LAYERS: PATHS HAS A PART FOR EVERY AI STATE, IN ITS COLOR");
    tour.click_row("LAYERS");
    tour.pause();
    click_rows(tour, {"VECTORS", "STEERING PROBES", "PATHS", "WANDER", "PURSUE", "KAMIKAZE", "RECOVERY",
                      "COLLISION SHAPES", "ACTIVE AGENTS", "AI STATES", "HIDING HITS"});
    tour.caption("ZOOMED OUT: WANDER, ATTACK, PURSUE, RECOVER AND KAMIKAZE");
    tour.step_row("ZOOM", false, 9);
    dotio_fight(tour, 9, false);

    tour.caption("ENEMIES: WANDER ALONG A PLANNED PATH INSTEAD OF BY STEERING");
    tour.click_row("ENEMIES");
    tour.click_row("WANDER");
    tour.click_row("PATH");
    dotio_fight(tour, 5, false);
    tour.click_row("WANDER");
    tour.click_row("STEERING");
    tour.caption("MOVE SPEED, FIRE INTERVAL AND THE FORCE OF EVERY STATE");
    tour.step_row("MOVE SPEED", true, 3);
    tour.step_row("FIRE INTERVAL", false, 2);
    dotio_fight(tour, 5, false);

    tour.caption("SPAWNING: MORE ENEMIES, SOONER");
    tour.click_row("SPAWNING");
    tour.step_row("ACTIVE LIMIT", true, 8);
    tour.step_row("SPAWN RATE", true, 6);
    dotio_fight(tour, 7, false);

    tour.caption("SIMULATION: DOUBLE SPEED, THEN TICK BY TICK");
    tour.click_row("SIMULATION");
    tour.step_row("SPEED", true, 1);
    dotio_fight(tour, 3, false);
    tour.step_row("SPEED", false, 1);
    tour.click_row("SINGLE TICK");
    click_again(tour, 7);
    tour.wait(0.5);
    tour.click_row("RUNNING");

    tour.caption("GHOST CAMERA: LOOK AROUND, CLICK TO TELEPORT");
    tour.click_row("GHOST CAMERA");
    tour.toggle_debug_menu();
    tour.hold({Key::d}, 1.3);
    tour.hold({Key::w}, 0.9);
    tour.click_at(free_teleport_point(tour));
    tour.wait(1.4);
    tour.toggle_debug_menu();
    tour.click_row("GHOST CAMERA");
    tour.caption("RESET PARAMETERS RESTORES THE DIFFICULTY PRESET");
    tour.click_row("RESET PARAMETERS");
    tour.step_row("ZOOM", true, 9);
    tour.toggle_debug_menu();
    dotio_fight(tour, 3, true);

    tour.caption("DIFFICULTY, THEN BACK TO THE LAUNCHER");
    tour.open_settings(dotio_sim_lines, dotio_debug_lines);
    tour.pause(0.5, 0.8);
    tour.click_settings_action(SettingsOverlay::leave_x);
    tour.wait(0.9);
    tour.click_menu_footer(kinetra::FooterButton::launcher, difficulty_rows);
    tour.wait(0.9);
}

// i4sim.

/// Row of `file` on a page that lists `files` after `rows_before` other rows. Throws
/// std::runtime_error when the catalog has no such file.
std::size_t row_of(const std::vector<std::filesystem::path> &files, std::string_view file,
                   std::size_t rows_before) {
    const auto found = std::find_if(files.begin(), files.end(),
                                    [&](const auto &path) { return path.filename().string() == file; });
    if (found == files.end())
        throw std::runtime_error("The i4sim catalog has no " + std::string(file));
    return rows_before + static_cast<std::size_t>(found - files.begin());
}

void i4sim_chapter(Tour &tour) {
    const auto directory = i4sim::default_blueprint_directory();
    const auto floors = i4sim::discover_blueprints(directory);
    const auto scenes = i4sim::discover_blueprints(directory / "showcase");
    // The loadscreen lists SHOWCASE first, then the floors.
    const std::size_t catalog_rows = floors.size() + 1;

    tour.caption("I4SIM: A FACTORY FLOOR WITH MOVERS AND PRODUCERS");
    tour.click_menu_page_row(2, launcher_rows);
    tour.wait(1.4);
    tour.caption("SHOWCASE: SMALL SCENES WITH THEIR OWN SETUP");
    tour.click_menu_page_row(0, catalog_rows);
    tour.pause(0.7, 1.0);
    tour.click_menu_page_row(row_of(scenes, showcase_scene, 0), scenes.size());
    tour.wait(1.2);
    tour.caption("H CROSSING: FOUR MOVERS SWAP CORNERS WITHOUT PATH PLANNING");
    tour.tap({Key::space});
    tour.wait(11.0);
    tour.caption("BLUEPRINTS IN THE SETTINGS RETURNS TO THE CATALOG");
    tour.open_settings(i4sim_sim_lines, i4sim_debug_lines);
    tour.pause(0.6, 0.9);
    tour.click_settings_action(SettingsOverlay::leave_x);
    tour.wait(1.2);

    tour.caption("A FLOOR OF 4320 BY 2400 MM: 29 MOVERS, 18 PRODUCERS");
    tour.click_menu_page_row(row_of(floors, floor_blueprint, 1), catalog_rows);
    tour.wait(1.2);
    // The floor's MOTION and SIMULATION rows come later; BLUEPRINTS is no longer in the debug
    // menu, so the menu is opened here.
    tour.toggle_debug_menu();
    tour.wait(0.6);
    tour.caption("A CLICK INSPECTS A MOVER, SHIFT + CLICK GIVES IT A GOAL");
    // The floor loads paused, so its movers still stand where the blueprint puts them.
    const auto blueprint = i4sim::load_blueprint(directory / floor_blueprint);
    if (blueprint.movers.empty())
        throw std::runtime_error("The tour's i4sim floor has no movers");
    // A point inside the footprint, whose top-left corner the blueprint gives in millimeters.
    const Vec2 inside_footprint{120, 120};
    // The mover farthest to the lower right: the debug menu covers the upper left.
    Vec2 mover{};
    for (const auto &definition : blueprint.movers) {
        const Vec2 point =
            tour.camera().world_to_screen(i4sim::flip_y(definition.position + inside_footprint));
        if (point.x + point.y > mover.x + mover.y)
            mover = point;
    }
    tour.click_at(mover);
    tour.wait(1.6);
    tour.set_held({Key::shift});
    tour.click_at(at(tour, .44f, .62f));
    tour.set_held({});
    tour.wait(0.8);
    tour.caption("SPACE STARTS THE FLOOR");
    tour.tap({Key::space});
    tour.wait(5.0);

    tour.caption("MOTION: THE STEERING PROFILE AND THE PLANNER");
    tour.click_row("MOTION");
    tour.click_row("PROFILE");
    tour.click_row("RECIPROCAL");
    tour.click_row("PLANNER");
    tour.click_row("A*");
    tour.wait(4.0);
    tour.step_row("SPEED", true, 2);
    tour.wait(3.0);
    tour.caption("SIMULATION: FOUR TIMES THE UPDATE RATE");
    tour.click_row("SIMULATION");
    tour.step_row("UPS", true, 2);
    tour.wait(5.0);
    tour.step_row("UPS", false, 2);
    tour.caption("LAYERS: ROUTES, FORCES, BORDERS AND DETECTION, PART BY PART");
    tour.click_row("LAYERS");
    tour.pause();
    // HEATMAP starts off, so hiding DETECTION and FORCES leaves routes and borders.
    click_rows(tour, {"DETECTION", "FORCES"});
    tour.wait(2.2);
    click_rows(tour, {"FORCES", "DETECTION"});
    tour.wait(1.0);
    tour.caption("TRAFFIC: JUNCTIONS AND CORRIDORS ADMIT ONE DIRECTION AT A TIME");
    tour.click_row("TRAFFIC");
    click_rows(tour, {"JUNCTION TRAFFIC", "CORRIDOR TRAFFIC"});
    tour.wait(5.0);

    tour.caption("THE WHEEL ZOOMS, W A S D MOVES THE VIEW");
    tour.move_to(at(tour, .56f, .52f));
    tour.scroll(9);
    tour.hold({Key::d}, 1.2);
    tour.hold({Key::s}, 0.8);
    tour.wait(2.0);
    tour.caption("FRAME SCENE FITS THE FLOOR IN THE WINDOW AGAIN");
    tour.click_row("FRAME SCENE");
    tour.wait(1.6);
    tour.toggle_debug_menu();

    tour.caption("DEBUG MODE OFF: THE FLOOR AS AN OPERATOR SEES IT");
    tour.open_settings(i4sim_sim_lines, i4sim_debug_lines);
    tour.pause(0.5, 0.8);
    tour.click_setting(SettingsRow::debug_mode);
    tour.pause(0.4, 0.6);
    tour.click_settings_action(SettingsOverlay::resume_x);
    tour.wait(4.5);

    tour.caption("BLUEPRINTS, LAUNCHER AND EXIT");
    tour.open_settings(i4sim_sim_lines, i4sim_debug_lines);
    tour.pause(0.5, 0.8);
    tour.click_settings_action(SettingsOverlay::leave_x);
    tour.wait(1.0);
    tour.click_menu_footer(kinetra::FooterButton::launcher, catalog_rows);
    tour.wait(1.0);
    tour.caption("");
    tour.move_to(
        tour.menu_layout(launcher_rows).footer_center(*tour.menu().footer_x(kinetra::FooterButton::exit)) +
        Vec2{0, 2});
    tour.pause(0.5, 0.8);
    tour.click();
}
} // namespace

const demo::Application &demo_application(Tour &tour) {
    return dynamic_cast<demo::Session &>(tour.session()).application();
}

Vec2 nearest_demo_agent(Tour &tour, Vec2 near) {
    const auto &scene = demo_application(tour).scene();
    std::optional<Vec2> best;
    for (const auto &agent : scene.agents()) {
        if (!agent.active())
            continue;
        const Vec2 point = tour.camera().world_to_screen(scene.world().body(agent.body_id()).position);
        if (!best || kinetra::length(point - near) < kinetra::length(*best - near))
            best = point;
    }
    return best.value_or(near);
}

void require_demo_terrain(kinetra_lab::Session *running) {
    // One message for every way the launcher chapter can go wrong, so the smoke test's
    // report says which click moved instead of a bad cast.
    const std::string failed = "The tour did not start Demo Terrain: ";
    if (!running)
        throw std::runtime_error(failed + "the launcher still shows a menu page");
    const auto *demo_session = dynamic_cast<const demo::Session *>(running);
    if (!demo_session)
        throw std::runtime_error(failed + running->title() + " runs instead");
    if (const auto scenario = demo_session->application().scene().scenario();
        scenario != demo::Scenario::terrain)
        throw std::runtime_error(failed + "Demo runs " + std::string(demo::scenario_label(scenario)) +
                                 " instead");
}

void play_tour(Tour &tour, const std::function<void()> &terrain_started) {
    launcher_chapter(tour);
    require_demo_terrain(tour.in_menu() ? nullptr : &tour.session());
    terrain_started();
    demo_chapter(tour);
    dotio_chapter(tour);
    i4sim_chapter(tour);
    tour.fade_out(1.2);
}
} // namespace capture
