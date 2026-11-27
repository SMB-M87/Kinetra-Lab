// Session tests for blueprint-screen messages, the mover inspector, and the rendered
// controls and traffic overlays. CTest labels the suite `graphics`.
//
// Set I4SIM_CAPTURE=<file.ppm> to save the frame after the Motion checks.
#include "i4sim/blueprint/blueprint.h"
#include "i4sim/presentation/session.h"
#include "i4sim/testing/showcase.h"
#include "kinetra_lab/inspector.h"

#include "kinetra_lab/settings.h"
#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/launcher.h>
#include <kinetra/runtime/overlay_style.h>
#include <kinetra/runtime/settings_view.h>

#include <SDL2/SDL.h>
#include <doctest/doctest.h>
#include <glad/glad.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace kinetra;

namespace {
using CreateSession = std::unique_ptr<kinetra_lab::Session> (*)(std::uint32_t, std::filesystem::path,
                                                                std::optional<std::filesystem::path>, bool,
                                                                unsigned);
static_assert(std::is_same_v<decltype(&i4sim::create_session), CreateSession>);

/// Writes the showcase blueprint `name` without its scenario section into `directory`, so
/// the test builds the scene through the debug menu and mouse goals instead. The section is
/// the last property of every showcase file.
std::filesystem::path without_scenario(std::string_view name, const std::filesystem::path &directory) {
    std::ifstream in(i4sim::test::showcase_blueprint(name));
    const std::string json{std::istreambuf_iterator<char>(in), {}};
    // Cut at the comma before the key, so the result is valid JSON with any line endings.
    const auto key = json.find("\"scenario\"");
    REQUIRE_MESSAGE(key != std::string::npos, "Showcase blueprint lacks its scenario section");
    const auto section = json.rfind(',', key);
    std::filesystem::create_directories(directory);
    const auto file = directory / (std::string(name) + ".json");
    std::ofstream(file) << json.substr(0, section) << "\n}\n";
    return file;
}

/// Rows of the debug menu (FactoryController::menu), counted from 0 below its heading. The
/// menu opens with SIMULATION open; opening a section closes the open one, so the open
/// section's rows always start right below its heading.
namespace menu_row {
/// The SIMULATION heading, and the LAYERS and MOTION headings while SIMULATION, with its
/// five rows, is open.
constexpr unsigned simulation = 0, layers_below_simulation = 6, motion_below_simulation = 7;
/// LAYERS open: heading on row 1, then GRID, HEATMAP and PATHS.
constexpr unsigned layers = 1, paths = 4;
/// MOTION open: heading on row 2, then PROFILE, PLANNER, RECOVERY, SPEED, FORCE, HORIZON and
/// RESET PARAMETERS, then the TRAFFIC heading.
constexpr unsigned motion = 2, profile = 3, planner = 4, speed = 6, force = 7, horizon = 8,
                   traffic_below_motion = 10;
/// The values of the open PROFILE list, below its row.
constexpr unsigned improved = 4, original = 5, reciprocal = 7;
/// The values of the open PLANNER list, below its row.
constexpr unsigned plain_a_star = 6;
/// TRAFFIC open: heading on row 3, then its four check boxes.
constexpr unsigned corridor = 4, junction = 5, parking = 6, predictive = 7;
} // namespace menu_row

/// Window position of a point given in the panel pixels of a Lab settings panel with its
/// help collapsed, located through a panel with the same Lab rows, so the tests follow the
/// shared layout.
Vec2 settings_point(Vec2 viewport, float x, float y) {
    bool statistics{}, debug{}, frame_times{};
    const SettingsOverlay mirror(
        kinetra_lab::settings_rows(statistics, debug, frame_times, {"LINE"}, {"LINE"}));
    return viewport / 2 + (Vec2{x, y} - mirror.half_extent()) * SettingsOverlay::scale(viewport);
}

/// The settings panel's action row, at the button centered on `x`.
Vec2 settings_action(Vec2 viewport, float x) {
    bool statistics{}, debug{}, frame_times{};
    const SettingsOverlay mirror(
        kinetra_lab::settings_rows(statistics, debug, frame_times, {"LINE"}, {"LINE"}));
    return settings_point(viewport, x, mirror.actions_y() + 3);
}

/// Opens the settings panel with Ctrl and clicks `point` in it.
void click_in_settings(kinetra_lab::Session &session, Camera2D &camera, Vec2 point) {
    Input open;
    open.pressed[std::size_t(Key::control)] = true;
    session.advance(0, open, camera);
    Input click;
    click.clicked = true;
    click.mouse = point;
    session.advance(0, click, camera);
}

/// Window position of `button` in the footer of the blueprint screen for the default
/// catalog, on its blueprint list or on its SHOWCASE page, located through a menu with the
/// same rows.
Vec2 blueprint_footer(Vec2 viewport, FooterButton button, bool showcase_page) {
    const auto directory = i4sim::default_blueprint_directory();
    std::vector<LaunchChoice> rows{
        {"SHOWCASE", "",
         std::vector<std::string>(i4sim::discover_blueprints(directory / "showcase").size(), "S"), 0}};
    for (std::size_t i = 0; i < i4sim::discover_blueprints(directory).size(); ++i)
        rows.push_back({"BLUEPRINT", "", {}, 0});
    LaunchMenu mirror(rows, "", LauncherButton::every_page);
    if (showcase_page)
        mirror.show_configurations({0, 0});
    const auto x = mirror.footer_x(button);
    REQUIRE(x);
    return mirror.layout(viewport).footer_center(*x);
}
} // namespace

TEST_CASE("a rejected goal expires on the blueprint screen") {
    auto session = i4sim::create_session(42, i4sim::default_blueprint_directory(),
                                         i4sim::test::showcase_blueprint("corridor-passing"));
    Camera2D camera;
    camera.viewport = {1280, 800};
    session->advance(0, {}, camera);

    Input hold;
    hold.right_clicked = true;
    hold.mouse = camera.world_to_screen({300, -700});
    session->advance(0, hold, camera);
    Input goal;
    goal.clicked = true;
    goal.held[std::size_t(Key::shift)] = true;
    goal.mouse = camera.world_to_screen({1300, -700});
    session->advance(0, goal, camera);

    click_in_settings(*session, camera, settings_action(camera.viewport, SettingsOverlay::leave_x));
    session->advance(0, {}, camera);
    REQUIRE(session->status().starts_with("Loadscreen | GOAL REJECTED"));

    session->advance(3.9, {}, camera);
    CHECK(session->status().starts_with("Loadscreen | GOAL REJECTED"));
    session->advance(0.2, {}, camera);
    CHECK(session->status() == "Loadscreen");
}

TEST_CASE("BLUEPRINTS in the settings panel closes the blueprint, and the next one stays") {
    auto session = i4sim::create_session(42, i4sim::default_blueprint_directory(),
                                         i4sim::test::showcase_blueprint("corridor-passing"));
    Camera2D camera;
    camera.viewport = {1280, 800};
    session->advance(0, {}, camera);
    REQUIRE_FALSE(session->status().starts_with("Loadscreen"));
    // The button is the settings panel's, so it works with the debug mode off: STATISTICS is
    // row 0 and DEBUG MODE row 1 of the Lab rows.
    bool statistics{}, debug{}, frame_times{};
    const SettingsOverlay mirror(
        kinetra_lab::settings_rows(statistics, debug, frame_times, {"LINE"}, {"LINE"}));
    click_in_settings(*session, camera, settings_point(camera.viewport, 60, mirror.row_y(1) + 3));
    Input resume;
    resume.clicked = true;
    resume.mouse = settings_action(camera.viewport, SettingsOverlay::resume_x);
    session->advance(0, resume, camera);
    click_in_settings(*session, camera, settings_action(camera.viewport, SettingsOverlay::leave_x));
    CHECK_MESSAGE(session->request() == SessionRequest::none, "BLUEPRINTS stays in i4sim");
    CHECK_FALSE_MESSAGE(session->status().starts_with("Loadscreen"),
                        "the frame of the click keeps its blueprint");
    session->advance(0, {}, camera);
    CHECK(session->status() == "Loadscreen");
    CHECK(session->request() == SessionRequest::none);

    Input choose;
    choose.pressed[std::size_t(Key::enter)] = true;
    session->advance(0, choose, camera); // the SHOWCASE page
    session->advance(0, choose, camera); // its first scene
    session->advance(0, {}, camera);
    session->advance(0, {}, camera);
    CHECK_MESSAGE(!session->status().starts_with("Loadscreen"), "the request closed the next blueprint too");
    CHECK(session->request() == SessionRequest::none);
}

TEST_CASE("LAUNCHER on both pages of the blueprint screen leaves for the launcher") {
    Camera2D camera;
    camera.viewport = {1280, 800};
    for (const bool showcase_page : {false, true}) {
        CAPTURE(showcase_page);
        auto session = i4sim::create_session(42, i4sim::default_blueprint_directory());
        session->advance(0, {}, camera);
        REQUIRE(session->status() == "Loadscreen");
        if (showcase_page) {
            Input choose;
            choose.pressed[std::size_t(Key::enter)] = true;
            session->advance(0, choose, camera);
        }
        Input click;
        click.clicked = true;
        click.mouse = blueprint_footer(camera.viewport, FooterButton::launcher, showcase_page);
        session->advance(0, click, camera);
        CHECK(session->request() == SessionRequest::menu);
        auto exit = i4sim::create_session(42, i4sim::default_blueprint_directory());
        exit->advance(0, {}, camera);
        if (showcase_page) {
            Input choose;
            choose.pressed[std::size_t(Key::enter)] = true;
            exit->advance(0, choose, camera);
        }
        click.mouse = blueprint_footer(camera.viewport, FooterButton::exit, showcase_page);
        exit->advance(0, click, camera);
        CHECK(exit->request() == SessionRequest::exit);
    }
}

TEST_CASE("the mover inspector and debug pause work together") {
    Runtime runtime("i4sim inspector", 1280, 800);
    runtime.read_back(true);
    auto session = i4sim::create_session(42, i4sim::default_blueprint_directory(),
                                         i4sim::test::showcase_blueprint("corridor-passing"));
    session->render(runtime);
    Input debug_menu;
    debug_menu.pressed[std::size_t(Key::shift)] = debug_menu.pressed[std::size_t(Key::control)] = true;
    session->advance(0, debug_menu, runtime.camera);
    const auto click = [&](Vec2 point, bool shift = false) {
        Input input;
        input.clicked = true;
        input.mouse = point;
        input.held[std::size_t(Key::shift)] = shift;
        session->advance(0, input, runtime.camera);
    };
    const auto title_visible = [&] {
        session->render(runtime);
        const auto rgb = runtime.frame().pixel(1238, 20);
        const auto title = overlay::title_color;
        return std::abs(float(rgb[0]) - title.r * 255) < 2 && std::abs(float(rgb[1]) - title.g * 255) < 2 &&
               std::abs(float(rgb[2]) - title.b * 255) < 2;
    };
    click(runtime.camera.world_to_screen({300, -700}));
    CHECK(title_visible());
    const float zoom_before_scroll = runtime.camera.pixels_per_unit;
    Input scroll;
    scroll.mouse = {1220, 70};
    scroll.wheel = -3;
    session->advance(0, scroll, runtime.camera);
    CHECK(runtime.camera.pixels_per_unit == zoom_before_scroll);
    click(runtime.camera.world_to_screen({1900, -700}));
    CHECK(title_visible());
    click(runtime.camera.world_to_screen({1300, -700}), true);
    CHECK(title_visible());
    const auto running =
        Vec2{8, 32} + Vec2{12, DebugMenu::row_top(1) + 8} * DebugMenu::scale(runtime.camera.viewport);
    click(running);
    CHECK(session->status().ends_with("RUNNING"));
    click(running);
    CHECK(session->status().ends_with("PAUSED"));
    CHECK(title_visible());
    click({1260, 20});
    session->render(runtime);
    CHECK_FALSE(title_visible());
}

TEST_CASE("the inspector and frame graph fit alongside debug in a narrow window") {
    Runtime runtime("Narrow inspector", 320, 240);
    runtime.read_back(true);
    DebugMenu menu;
    menu.visible = true;
    menu.screen_origin = {8, 32};
    bool running = true;
    const auto entries = std::array{debug_check("RUNNING", running)};
    InspectorView view;
    const InspectorSnapshot snapshot{
        "MOVER 0", {{"MOTION", {{"LONG VALUE", std::string(100, 'A')}, {"LAST", "0 MM/TICK"}}}}};
    FrameTimes times;
    for (std::size_t i = 0; i < FrameTimes::capacity; ++i)
        times.record(0.01);
    const auto viewport = runtime.camera.viewport;
    const auto area = kinetra_lab::inspector_area(menu, viewport, true);
    const auto box = view.bounds(snapshot, area);
    const float scale = kinetra_lab::inspector_graph_scale(menu, viewport);
    REQUIRE(scale < 1);
    REQUIRE(box.min.x >= area.min.x);
    runtime.begin({0, 0, 0});
    menu.draw(runtime, entries);
    kinetra_lab::draw_inspector(runtime, menu, &view, &snapshot, &times);
    runtime.end();
    const float green_y = box.max.y + 12 + (FrameTimeGraph::height - 4) * scale;
    const auto pixel = runtime.frame().pixel(295, static_cast<int>(std::ceil(green_y)));
    CHECK((pixel[0] < 120 && pixel[1] > 180 && pixel[2] < 140));
    Input click;
    click.clicked = true;
    click.mouse = menu.screen_origin + Vec2{16, DebugMenu::row_top(0) + 8} * DebugMenu::scale(viewport);
    CHECK_FALSE(view.update(click, snapshot, area).captured);
    CHECK(menu.update(click, viewport, entries));
    CHECK_FALSE(running);
    click.mouse = {box.max.x - 12, box.min.y + 12};
    CHECK(view.update(click, snapshot, area).close);
}

TEST_CASE("the session renders its overlays and handles input and catalog recovery") {
    Runtime runtime("i4sim presentation", 640, 480);
    runtime.read_back(true);
    auto session = i4sim::create_session();
    session->advance(0, {}, runtime.camera);
    session->render(runtime);
    CHECK_MESSAGE(session->status().find("Loadscreen") != std::string::npos, "Missing loadscreen");
    Input choose;
    choose.pressed[std::size_t(Key::enter)] = true;
    Input down;
    down.pressed[std::size_t(Key::down)] = true;
    session->advance(0, down, runtime.camera); // past SHOWCASE to the first blueprint
    session->advance(0, choose, runtime.camera);
    session->render(runtime);
    CHECK_MESSAGE(session->status().find("PAUSED") != std::string::npos,
                  "Loadscreen did not load paused blueprint");
    Input run;
    run.pressed[std::size_t(Key::space)] = true;
    session->advance(0, run, runtime.camera);
    for (Key key : {Key::one, Key::two, Key::three, Key::four, Key::five, Key::six}) {
        Input toggle;
        toggle.pressed[std::size_t(key)] = true;
        session->advance(1.0 / 60, toggle, runtime.camera);
    }
    session->render(runtime);
    CHECK_MESSAGE(session->status().find("RUNNING") != std::string::npos, "Pause control failed");
    // Shift+Ctrl opens and closes the debug menu.
    Input debug_menu;
    debug_menu.pressed[std::size_t(Key::shift)] = debug_menu.pressed[std::size_t(Key::control)] = true;
    session->advance(0, debug_menu, runtime.camera);
    // A click on the label of menu row `row`, or at panel x `x` on it.
    auto click_row = [&](unsigned row, float x = 12) {
        Input click;
        click.clicked = true;
        click.mouse = Vec2{8, 32} + Vec2{x, DebugMenu::row_top(row) + DebugMenu::row_height / 2} *
                                        DebugMenu::scale(runtime.camera.viewport);
        session->advance(0, click, runtime.camera);
    };
    // A click on the [+] button of menu row `row`, or with `up` false on its [-] button.
    auto click_step = [&](unsigned row, bool up = true) {
        click_row(row,
                  (up ? DebugMenu::increase_left : DebugMenu::decrease_left) + DebugMenu::stepper_width / 2);
    };
    // Pixels inside the debug menu panel that match `color`, in the 16 rows below its heading.
    auto menu_pixels = [&](auto color) {
        session->render(runtime);
        const auto &frame = runtime.frame();
        const float scale = DebugMenu::scale(runtime.camera.viewport);
        const int right = int(8 + DebugMenu::panel_width * scale),
                  bottom = int(32 + DebugMenu::row_top(16) * scale);
        unsigned count = 0;
        for (int y = 32; y < bottom; ++y)
            for (int x = 8; x < right; ++x) {
                const auto pixel = frame.pixel(x, y);
                count += color(pixel[0], pixel[1], pixel[2]);
            }
        return count;
    };
    const auto contact_red = [](int r, int g, int b) { return r > 240 && g < 20 && b < 20; };
    const auto route_purple = [](int r, int g, int b) {
        return r > 150 && r < 200 && g > 80 && g < 125 && b > 230;
    };
    // The keys above turned PATHS off: its parts are not listed until it is on again.
    click_row(menu_row::layers_below_simulation);
    CHECK_MESSAGE(menu_pixels(contact_red) > 50, "The CONTACT SHAPES row has no color swatch");
    CHECK_MESSAGE(menu_pixels(route_purple) == 0, "PATHS lists its parts while it is off");
    click_row(menu_row::paths);
    CHECK_MESSAGE(menu_pixels(route_purple) > 50, "PATHS does not list ROUTES with its swatch");
    click_row(menu_row::paths);
    click_row(menu_row::layers);
    click_row(menu_row::simulation);
    click_row(menu_row::motion_below_simulation);
    // The bottom 40 rows, where the statistics lines are.
    auto statistics_pixels = [&] {
        session->render(runtime);
        const auto &rgba = runtime.frame().rgba;
        return std::vector<std::uint8_t>(rgba.end() - 640 * 40 * 4, rgba.end());
    };
    const auto before_motion = statistics_pixels();
    click_step(menu_row::speed); // Speed + must apply while running.
    CHECK_MESSAGE(statistics_pixels() != before_motion, "Running Motion edit did not update statistics");
    click_step(menu_row::speed, false); // Speed - restores the initial speed.
    CHECK_MESSAGE(statistics_pixels() == before_motion, "Inverse Motion edit did not restore speed");
    click_row(menu_row::profile);
    click_row(menu_row::reciprocal);
    CHECK_MESSAGE(statistics_pixels() != before_motion, "Reciprocal profile did not expose avoidance limits");
    click_row(menu_row::profile);
    click_row(menu_row::improved);
    CHECK_MESSAGE(statistics_pixels() == before_motion, "Profile list did not restore Improved");
    click_row(menu_row::traffic_below_motion);
    click_row(menu_row::parking);
    CHECK_MESSAGE(statistics_pixels() != before_motion, "Parking traffic did not expose wait statistics");
    click_row(menu_row::parking);
    CHECK_MESSAGE(statistics_pixels() == before_motion,
                  "Parking traffic did not restore disabled statistics");
    click_row(menu_row::junction);
    CHECK_MESSAGE(statistics_pixels() != before_motion, "Junction traffic did not expose wait statistics");
    click_row(menu_row::junction);
    CHECK_MESSAGE(statistics_pixels() == before_motion, "Junction traffic did not clear disabled statistics");
    click_row(menu_row::corridor);
    CHECK_MESSAGE(statistics_pixels() != before_motion, "Corridor traffic did not expose wait statistics");
    click_row(menu_row::corridor);
    CHECK_MESSAGE(statistics_pixels() == before_motion, "Corridor traffic did not clear disabled statistics");
    click_row(menu_row::predictive); // Predictive passing is an explicit scene policy.
    CHECK_MESSAGE(statistics_pixels() != before_motion, "Passing toggle did not update statistics");
    click_row(menu_row::predictive);
    CHECK_MESSAGE(statistics_pixels() == before_motion, "Passing toggle did not restore disabled statistics");
    for (unsigned row = menu_row::corridor; row <= menu_row::predictive; ++row) {
        click_row(row);
        CHECK_MESSAGE(session->status().find("RUNNING") != std::string::npos,
                      "Traffic edit did not preserve running state");
    }
    click_row(menu_row::motion);
    for (const unsigned row : {menu_row::speed, menu_row::force, menu_row::horizon}) {
        click_step(row);
        CHECK_MESSAGE(session->status().find("RUNNING") != std::string::npos,
                      "Motion edit did not preserve running state");
    }
    session->advance(0, run, runtime.camera);
    click_row(menu_row::profile);
    click_row(menu_row::original);
    CHECK_MESSAGE(session->status().find("PAUSED") != std::string::npos,
                  "Motion edit resumed a paused simulation");
    session->advance(0, run, runtime.camera);
    session->advance(0, debug_menu, runtime.camera);
    SDL_Event event{};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_RIGHT;
    CHECK_MESSAGE((SDL_PushEvent(&event) == 1 && runtime.poll(false) && runtime.input().right_clicked &&
                   runtime.input().right_mouse_down),
                  "Right button was not exposed");
    CHECK_MESSAGE((runtime.poll(false) && !runtime.input().right_clicked && runtime.input().right_mouse_down),
                  "Right click was not frame-scoped");
    event.type = SDL_MOUSEBUTTONUP;
    CHECK_MESSAGE((SDL_PushEvent(&event) == 1 && runtime.poll(false) && !runtime.input().right_mouse_down),
                  "Right button did not release");
    if (const char *capture = std::getenv("I4SIM_CAPTURE")) {
        const auto &frame = runtime.frame();
        std::ofstream file(capture, std::ios::binary);
        file << "P6\n" << frame.width << " " << frame.height << "\n255\n";
        for (std::size_t pixel = 0; pixel < frame.rgba.size(); pixel += 4)
            file.write(reinterpret_cast<const char *>(frame.rgba.data() + pixel), 3);
    }
    CHECK_MESSAGE(glGetError() == GL_NO_ERROR, "i4sim rendering produced OpenGL error");
    Input panel;
    panel.pressed[std::size_t(Key::control)] = true;
    session->advance(0, panel, runtime.camera);
    CHECK_MESSAGE(session->status().find("PAUSED") != std::string::npos,
                  "Settings did not suspend simulation");
    Input suppressed;
    suppressed.pressed[std::size_t(Key::space)] = true;
    suppressed.clicked = true;
    suppressed.mouse = {1, 479};
    session->advance(1, suppressed, runtime.camera);
    CHECK_MESSAGE(session->status().find("PAUSED") != std::string::npos,
                  "Modal settings leaked simulation input");
    session->advance(0, panel, runtime.camera);
    CHECK_MESSAGE(session->status().find("RUNNING") != std::string::npos,
                  "Settings did not restore running state");
    Input back;
    back.pressed[std::size_t(Key::control)] = true;
    session->advance(0, back, runtime.camera);
    session->render(runtime);
    back = {};
    // Centered modal settings, default collapsed help: BLUEPRINTS, then the blueprint
    // screen's LAUNCHER.
    back.mouse = settings_action(runtime.camera.viewport, SettingsOverlay::leave_x);
    back.clicked = true;
    session->advance(0, back, runtime.camera);
    session->advance(0, {}, runtime.camera);
    CHECK_MESSAGE(session->status() == "Loadscreen", "BLUEPRINTS did not return to the blueprint screen");
    session->render(runtime);
    back.mouse = blueprint_footer(runtime.camera.viewport, FooterButton::launcher, false);
    session->advance(0, back, runtime.camera);
    CHECK_MESSAGE(session->request() == SessionRequest::menu, "Cannot return to launcher");
    session = i4sim::create_session(42, i4sim::default_blueprint_directory(),
                                    i4sim::default_blueprint_directory() / "missing.json");
    session->render(runtime);
    CHECK_MESSAGE(session->status().find("Loadscreen") != std::string::npos,
                  "Failed load left a partial simulation");
    const auto repaired_catalog =
        std::filesystem::temp_directory_path() /
        ("i4sim-catalog-recovery-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    session = i4sim::create_session(42, repaired_catalog);
    CHECK_MESSAGE(session->status().find(repaired_catalog.string()) != std::string::npos,
                  "Missing catalog error does not identify its directory");
    session->render(runtime);
    std::filesystem::create_directories(repaired_catalog);
    std::filesystem::copy_file(i4sim::test::fixture_blueprint("MAS-demonstrator"),
                               repaired_catalog / "factory.json");
    session->advance(0, choose, runtime.camera);
    CHECK_MESSAGE(session->status() == "Loadscreen", "Enter did not rediscover a repaired catalog");
    session->advance(0, choose, runtime.camera);
    CHECK_MESSAGE(session->status().find("PAUSED") != std::string::npos,
                  "Repaired catalog did not load its blueprint");
    session->render(runtime);
    CHECK_MESSAGE(glGetError() == GL_NO_ERROR, "Catalog recovery rendering produced an OpenGL error");
    const auto crossing_file = without_scenario("junction-crossing", repaired_catalog / "manual");
    session = i4sim::create_session(42, repaired_catalog, crossing_file);
    session->render(runtime);
    session->advance(0, debug_menu, runtime.camera);
    click_row(menu_row::motion_below_simulation);
    click_row(menu_row::profile);
    click_row(menu_row::reciprocal);
    click_row(menu_row::traffic_below_motion);
    click_row(menu_row::junction);
    session->advance(0, debug_menu, runtime.camera);
    Input no_workload;
    no_workload.pressed[std::size_t(Key::g)] = true;
    session->advance(0, no_workload, runtime.camera);
    for (int i = 0; i < 2; ++i) {
        Input select;
        select.clicked = true;
        select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{300, -700} : Vec2{700, -300});
        session->advance(0, select, runtime.camera);
        select.held[std::size_t(Key::shift)] = true;
        select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1100, -700} : Vec2{700, -1100});
        session->advance(0, select, runtime.camera);
    }
    session->advance(0, run, runtime.camera);
    for (int frame = 0; frame < 60; ++frame) {
        session->advance(0.02, {}, runtime.camera);
        session->render(runtime);
    }
    auto crossing_pixels = runtime.frame().rgba;
    unsigned cyan = 0, lime = 0;
    for (std::size_t i = 0; i < crossing_pixels.size(); i += 4) {
        if (crossing_pixels[i] < 30 && crossing_pixels[i + 1] > 180 && crossing_pixels[i + 2] > 240)
            ++cyan;
        if (crossing_pixels[i] > 120 && crossing_pixels[i] < 180 && crossing_pixels[i + 1] > 235 &&
            crossing_pixels[i + 2] < 30)
            ++lime;
    }
    CHECK_MESSAGE((lime > 30 && cyan > 100 && session->status().find("RUNNING") != std::string::npos &&
                   glGetError() == GL_NO_ERROR),
                  "Actual crossing/zone rendering failed");
    const auto corridor_file = without_scenario("corridor-passing", repaired_catalog / "manual");
    session = i4sim::create_session(42, repaired_catalog, corridor_file);
    session->render(runtime);
    session->advance(0, debug_menu, runtime.camera);
    click_row(menu_row::motion_below_simulation);
    click_row(menu_row::profile);
    click_row(menu_row::reciprocal);
    click_row(menu_row::traffic_below_motion);
    click_row(menu_row::corridor);
    session->advance(0, debug_menu, runtime.camera);
    session->advance(0, no_workload, runtime.camera);
    for (int i = 0; i < 2; ++i) {
        Input select;
        select.clicked = true;
        select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{300, -700} : Vec2{1900, -700});
        session->advance(0, select, runtime.camera);
        select.held[std::size_t(Key::shift)] = true;
        select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1900, -700} : Vec2{300, -700});
        session->advance(0, select, runtime.camera);
    }
    session->advance(0, run, runtime.camera);
    for (int frame = 0; frame < 60; ++frame) {
        session->advance(0.02, {}, runtime.camera);
        session->render(runtime);
    }
    crossing_pixels = runtime.frame().rgba;
    unsigned blue = 0;
    lime = 0;
    for (std::size_t i = 0; i < crossing_pixels.size(); i += 4) {
        if (crossing_pixels[i] > 30 && crossing_pixels[i] < 80 && crossing_pixels[i + 1] > 90 &&
            crossing_pixels[i + 1] < 160 && crossing_pixels[i + 2] > 240)
            ++blue;
        if (crossing_pixels[i] > 120 && crossing_pixels[i] < 180 && crossing_pixels[i + 1] > 235 &&
            crossing_pixels[i + 2] < 30)
            ++lime;
    }
    CHECK_MESSAGE((blue > 50 && lime > 30 && glGetError() == GL_NO_ERROR),
                  "Actual corridor admission/exit rendering failed");
    const auto recovery_file = without_scenario("corridor-recovery", repaired_catalog / "manual");
    session = i4sim::create_session(42, repaired_catalog, recovery_file);
    session->render(runtime);
    session->advance(0, debug_menu, runtime.camera);
    click_row(menu_row::motion_below_simulation);
    click_row(menu_row::profile);
    click_row(menu_row::reciprocal);
    click_row(menu_row::traffic_below_motion);
    click_row(menu_row::corridor);
    session->advance(0, debug_menu, runtime.camera);
    session->advance(0, no_workload, runtime.camera);
    for (int i = 0; i < 2; ++i) {
        Input select;
        select.clicked = true;
        select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1000, -700} : Vec2{1200, -700});
        session->advance(0, select, runtime.camera);
        select.held[std::size_t(Key::shift)] = true;
        select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1900, -700} : Vec2{300, -700});
        session->advance(0, select, runtime.camera);
    }
    session->advance(0, run, runtime.camera);
    for (int frame = 0; frame < 110; ++frame) {
        session->advance(0.02, {}, runtime.camera);
        session->render(runtime);
    }
    crossing_pixels = runtime.frame().rgba;
    unsigned retreat_pixels = 0;
    for (std::size_t i = 0; i < crossing_pixels.size(); i += 4)
        if (crossing_pixels[i] > 230 && crossing_pixels[i + 1] > 50 && crossing_pixels[i + 1] < 100 &&
            crossing_pixels[i + 2] > 150 && crossing_pixels[i + 2] < 210)
            ++retreat_pixels;
    CHECK_MESSAGE((retreat_pixels > 20 && glGetError() == GL_NO_ERROR),
                  "Actual corridor retreat rendering failed");
    const auto passing_file = without_scenario("predictive-passing", repaired_catalog / "manual");
    session = i4sim::create_session(42, repaired_catalog, passing_file);
    session->render(runtime);
    session->advance(0, debug_menu, runtime.camera);
    click_row(menu_row::motion_below_simulation);
    click_row(menu_row::profile);
    click_row(menu_row::reciprocal);
    click_row(menu_row::planner); // Plain A* keeps this controlled encounter head-on.
    click_row(menu_row::plain_a_star);
    click_row(menu_row::traffic_below_motion);
    click_row(menu_row::predictive);
    session->advance(0, debug_menu, runtime.camera);
    session->advance(0, no_workload, runtime.camera);
    for (int i = 0; i < 2; ++i) {
        Input select;
        select.clicked = true;
        select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{500, -800} : Vec2{1900, -800});
        session->advance(0, select, runtime.camera);
        select.held[std::size_t(Key::shift)] = true;
        select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1900, -800} : Vec2{500, -800});
        session->advance(0, select, runtime.camera);
    }
    session->advance(0, run, runtime.camera);
    unsigned passing_pixels = 0;
    for (int frame = 0; frame < 180 && passing_pixels <= 20; ++frame) {
        session->advance(0.02, {}, runtime.camera);
        session->render(runtime);
        crossing_pixels = runtime.frame().rgba;
        passing_pixels = 0;
        for (std::size_t i = 0; i < crossing_pixels.size(); i += 4)
            if (crossing_pixels[i] > 60 && crossing_pixels[i] < 100 && crossing_pixels[i + 1] > 240 &&
                crossing_pixels[i + 2] > 160 && crossing_pixels[i + 2] < 200)
                ++passing_pixels;
    }
    CHECK_MESSAGE((passing_pixels > 20 && glGetError() == GL_NO_ERROR),
                  "Actual predictive passing rendering failed");
    // Showcase: the first loadscreen row opens a page of scenes that load with their setup.
    session = i4sim::create_session();
    session->advance(0, choose, runtime.camera);
    CHECK_MESSAGE(session->status() == "Loadscreen", "SHOWCASE loaded a blueprint instead of its scenes");
    Input escape;
    escape.pressed[std::size_t(Key::escape)] = true;
    session->advance(0, escape, runtime.camera);
    CHECK_MESSAGE((session->status() == "Loadscreen" && session->request() == SessionRequest::none),
                  "Escape on the SHOWCASE page left i4sim instead of returning to the list");
    session->advance(0, choose, runtime.camera); // SHOWCASE is still highlighted
    session->render(runtime);
    session->advance(0, choose, runtime.camera); // first scene, alphabetically corridor-passing
    CHECK_MESSAGE(session->status() == "Single-lane corridor | PAUSED", "Showcase scene did not load paused");
    // The world area between the toolbar and the statistics lines; without a workload, only
    // the scenario's goals can move anything there: rows 80 to 359.
    auto world_pixels = [&] {
        session->render(runtime);
        const auto &rgba = runtime.frame().rgba;
        return std::vector<std::uint8_t>(rgba.begin() + 80 * 640 * 4, rgba.begin() + 360 * 640 * 4);
    };
    auto run_frames = [&](int frames) {
        for (int frame = 0; frame < frames; ++frame) {
            session->advance(0.02, {}, runtime.camera);
            session->render(runtime);
        }
    };
    session->advance(0, run, runtime.camera);
    auto before = world_pixels();
    run_frames(30);
    CHECK_MESSAGE(world_pixels() != before, "Showcase scene movers did not move to their goals");
    run_frames(30);
    crossing_pixels = runtime.frame().rgba;
    blue = 0;
    for (std::size_t i = 0; i < crossing_pixels.size(); i += 4)
        if (crossing_pixels[i] > 30 && crossing_pixels[i] < 80 && crossing_pixels[i + 1] > 90 &&
            crossing_pixels[i + 1] < 160 && crossing_pixels[i + 2] > 240)
            ++blue;
    // The scene's corridor traffic draws the same admission overlay as the clicked setup above.
    CHECK_MESSAGE((blue > 50 && session->status() == "Single-lane corridor | RUNNING"),
                  "Showcase scene did not run with its corridor traffic");
    Input reset;
    reset.pressed[std::size_t(Key::r)] = true;
    session->advance(0, reset, runtime.camera);
    CHECK_MESSAGE(session->status() == "Single-lane corridor | PAUSED", "R did not restart the scene");
    session->advance(0, run, runtime.camera);
    before = world_pixels();
    run_frames(30);
    CHECK_MESSAGE(world_pixels() != before, "R did not re-send the scene's goals");

    // A restarting scene (corridor-recovery, the second scene) starts again by itself once its
    // movers have stood still for the pause. They settle near tick 2200 and the scene restarts
    // at tick 2600, so around tick 3000 they move again; without a restart they stand still.
    session = i4sim::create_session();
    session->advance(0, choose, runtime.camera);
    session->advance(0, down, runtime.camera);
    session->advance(0, choose, runtime.camera);
    REQUIRE_MESSAGE(session->status() == "Corridor retreat recovery | PAUSED",
                    "Restarting scene did not load");
    session->advance(0, run, runtime.camera);
    const auto run_ticks = [&](int ticks) {
        // 0.25 s holds exactly 50 ticks at the default 200 ticks per second.
        for (int frame = 0; frame < ticks / 50; ++frame)
            session->advance(0.25, {}, runtime.camera);
    };
    run_ticks(3000);
    before = world_pixels();
    run_ticks(200);
    CHECK_MESSAGE(world_pixels() != before, "Restarting scene did not start again after its pause");
    CHECK_MESSAGE(session->status() == "Corridor retreat recovery | RUNNING", "A restart paused the scene");
}
