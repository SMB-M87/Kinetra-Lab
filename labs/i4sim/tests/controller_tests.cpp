// Window-free tests for the i4sim session's controller (labs/i4sim/include/controller.h):
// loading, keys, pointer, camera, modal pause and the debug menu's entries. The blueprints
// come from the source tree.
#include "i4sim/controller.h"
#include "i4sim/testing/showcase.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace kinetra;
using i4sim::FactoryController;
using i4sim::flip_y;
using i4sim::Layer;
using i4sim::PanelState;
using i4sim::RunState;
using kinetra::DebugEntry;

namespace {
/// The test environment (the reference's MAS demonstrator), a test fixture outside the catalog.
const auto factory = i4sim::test::fixture_blueprint("test-environment");
/// An open floor of 2400 by 1600 mm with two movers at (500, 800) and (1900, 800).
const auto open_floor = i4sim::test::showcase_blueprint("predictive-passing");
const Layer top_layers[]{Layer::grid,    Layer::heatmap,   Layer::paths,         Layer::forces,
                         Layer::borders, Layer::detection, Layer::contact_shapes};

Input key(Key pressed) {
    Input input;
    input.pressed[static_cast<std::size_t>(pressed)] = true;
    return input;
}

Camera2D camera_for(FactoryController &controller) {
    Camera2D camera;
    camera.viewport = {1280, 800};
    controller.update_camera(camera);
    return camera;
}

/// One frame in the session's order, without panels.
void frame(FactoryController &controller, Camera2D &camera, const Input &input = {}, double elapsed = 0,
           PanelState panels = {}) {
    REQUIRE(controller.begin_frame(elapsed));
    controller.suspend(false);
    controller.handle_input(elapsed, input, camera, panels);
}

/// A left click, or with `right` a right click, on factory position `point`.
Input click(const Camera2D &camera, Vec2 point, bool right = false) {
    Input input;
    input.mouse = camera.world_to_screen(flip_y(point));
    (right ? input.right_clicked : input.clicked) = true;
    return input;
}

std::vector<std::string> labels(FactoryController &controller) {
    std::vector<std::string> result;
    for (const auto &entry : controller.menu())
        result.emplace_back(entry.label);
    return result;
}

DebugEntry entry(FactoryController &controller, std::string_view label) {
    for (auto &candidate : controller.menu())
        if (candidate.label == label)
            return candidate;
    FAIL("The menu page has no entry " << label);
    return {};
}

/// Runs the entry `label` of the current menu, as a click on its row does.
void choose(FactoryController &controller, std::string_view label) {
    entry(controller, label).action();
}

/// Opens the menu section with the heading `label`, unless it is open already.
void open_section(FactoryController &controller, std::string_view label) {
    auto heading = entry(controller, label);
    REQUIRE(heading.kind == kinetra::DebugEntryKind::group);
    if (!heading.active())
        heading.action();
}

/// Clicks the [+] button of the number row `label`, or with `up` false its [-] button.
void step(FactoryController &controller, std::string_view label, bool up = true) {
    auto row = entry(controller, label);
    REQUIRE((row.decrease && row.increase));
    (up ? row.increase : row.decrease)();
}

/// Opens the list of the MOTION row `list` and chooses `value` from it.
void choose_value(FactoryController &controller, std::string_view list, std::string_view value) {
    open_section(controller, "MOTION");
    choose(controller, list);
    choose(controller, value);
}

Vec2 mover_position(const FactoryController &controller, std::size_t index) {
    return controller.simulation().movers().at(index).body.position;
}
} // namespace

TEST_CASE("a blueprint loads paused at the session's update rate with a camera fit pending") {
    FactoryController controller(42, false, 400);
    CHECK_FALSE(controller.loaded());
    REQUIRE(controller.load(factory));
    REQUIRE(controller.loaded());
    const auto &simulation = controller.simulation();
    CHECK(simulation.blueprint().name == "test-environment");
    CHECK(simulation.state() == RunState::paused);
    CHECK_MESSAGE(controller.awaiting_start(), "a paused blueprint shows the RUN button");
    CHECK(simulation.updates_per_second() == 400);
    CHECK(simulation.seed() == 42);
    CHECK(controller.message().empty());
    CHECK(controller.description().empty());
    CHECK_FALSE(controller.selected_mover());
    CHECK_FALSE(controller.selected_producer());

    Camera2D camera;
    camera.viewport = {1280, 800};
    camera.position = {-5, 7};
    controller.update_camera(camera);
    const auto floor = simulation.blueprint().dimension;
    const Vec2 expected{floor.x / 2, -floor.y / 2};
    CHECK_MESSAGE(camera.position == expected,
                  "Camera position (" << camera.position.x << ", " << camera.position.y << ") expected ("
                                      << expected.x << ", " << expected.y << ")");
    // 2160 by 1200 mm in 1280 by 800 pixels, of which 120 stay free for the toolbar and the
    // HUD: the height decides, with a tenth as margin.
    CHECK(camera.pixels_per_unit == doctest::Approx((800.0 - 120) / 1200 * 0.9));
    camera.position = {1, 2};
    controller.update_camera(camera);
    CHECK_MESSAGE(camera.position.x == 1, "the fit runs once per request");
}

TEST_CASE("a controller built running starts its blueprints running") {
    FactoryController controller(7, true, 200);
    REQUIRE(controller.load(factory));
    CHECK(controller.simulation().state() == RunState::running);
    CHECK(controller.simulation().seed() == 7);
    CHECK_FALSE(controller.awaiting_start());
}

TEST_CASE("an update rate outside 1 to 10000 is rejected") {
    CHECK_THROWS_AS(FactoryController(42, false, 0), std::invalid_argument);
    CHECK_THROWS_AS(FactoryController(42, false, 10001), std::invalid_argument);
    CHECK_NOTHROW(FactoryController(42, false, 1));
    CHECK_NOTHROW(FactoryController(42, false, 10000));
}

TEST_CASE("a failed load keeps the loaded blueprint and reports the path and the cause") {
    FactoryController controller(42, false, 200);
    const auto missing = factory.parent_path() / "missing.json";
    CHECK_FALSE(controller.load(missing));
    CHECK_FALSE(controller.loaded());
    CHECK(controller.message().find("missing.json") != std::string::npos);

    REQUIRE(controller.load(factory));
    CHECK_MESSAGE(controller.message().empty(), "a successful load clears the message");
    CHECK_FALSE(controller.load(missing));
    REQUIRE(controller.loaded());
    CHECK(controller.simulation().blueprint().name == "test-environment");
    CHECK(controller.message().find("missing.json") != std::string::npos);
    REQUIRE(controller.begin_frame(60));
    CHECK_MESSAGE(!controller.message().empty(), "a reported error does not expire");
    controller.clear_message();
    CHECK(controller.message().empty());
}

TEST_CASE("a showcase scene arrives with its description, settings and goals") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(i4sim::test::showcase_blueprint("junction-crossing")));
    const auto &simulation = controller.simulation();
    CHECK(controller.description().find("Junction traffic") != std::string_view::npos);
    CHECK(simulation.motion_settings().profile == i4sim::SteeringProfile::reciprocal);
    CHECK(simulation.motion_settings().junction_traffic);
    CHECK_FALSE(simulation.workload());
    for (const auto &mover : simulation.movers()) {
        CHECK(mover.moving());
        CHECK(mover.goal == i4sim::GoalKind::manual);
    }
    CHECK_MESSAGE(simulation.state() == RunState::running, "a scene plays without waiting for Space");
    CHECK_FALSE(controller.awaiting_start());
}

TEST_CASE("space and p pause and resume") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, key(Key::space));
    CHECK(controller.simulation().state() == RunState::running);
    CHECK_FALSE(controller.awaiting_start());
    frame(controller, camera, key(Key::p));
    CHECK(controller.simulation().state() == RunState::paused);
    CHECK_MESSAGE(!controller.awaiting_start(), "a later pause does not bring the RUN button back");
    frame(controller, camera);
    CHECK_MESSAGE(controller.simulation().state() == RunState::paused,
                  "a frame without the key changes nothing");
}

TEST_CASE("the run button is centered below the toolbar and fits a narrow viewport") {
    const auto box = i4sim::start_button({1280, 800});
    CHECK(box.min.x == 540);
    CHECK(box.max.x == 740);
    CHECK(box.min.y == 30);
    CHECK(box.max.y == 70);
    const auto narrow = i4sim::start_button({200, 300});
    CHECK(narrow.min.x == 16);
    CHECK(narrow.max.x == 184);
    CHECK(narrow.max.y - narrow.min.y == 40);
}

TEST_CASE("the run button leaves the fitted floor free") {
    FactoryController controller(42, false, 200);
    for (const Vec2 viewport : {Vec2{640, 480}, Vec2{1280, 800}, Vec2{1920, 1080}}) {
        REQUIRE(controller.load(factory)); // A load asks for a fit.
        Camera2D camera;
        camera.viewport = viewport;
        controller.update_camera(camera);
        const float floor_top = camera.world_to_screen({0, 0}).y;
        CHECK(i4sim::start_button(viewport).max.y < floor_top);
    }
}

TEST_CASE("a click on the run button starts a paused blueprint and reaches nothing below it") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    REQUIRE(controller.awaiting_start());
    // Pan so that mover 0 sits under the middle of the button.
    const auto box = i4sim::start_button(camera.viewport);
    camera.position +=
        flip_y(mover_position(controller, 0)) - camera.screen_to_world((box.min + box.max) / 2);
    Input on_button = click(camera, mover_position(controller, 0), true);
    REQUIRE(box.overlaps({on_button.mouse, on_button.mouse}));
    frame(controller, camera, on_button);
    CHECK_MESSAGE(controller.awaiting_start(), "a right click on the button does not start");
    CHECK_FALSE(controller.selected_mover());
    CHECK_FALSE(controller.simulation().movers()[0].held);

    const Input beside = click(camera, mover_position(controller, 0) + Vec2{0, 400});
    REQUIRE_FALSE(box.overlaps({beside.mouse, beside.mouse}));
    frame(controller, camera, beside);
    CHECK_MESSAGE(controller.awaiting_start(), "a click outside the button does not start");

    on_button.right_clicked = false;
    on_button.clicked = true;
    frame(controller, camera, on_button);
    CHECK(controller.simulation().running());
    CHECK_FALSE(controller.awaiting_start());
    CHECK_MESSAGE(!controller.selected_mover(), "the button's click selects nothing");

    frame(controller, camera, key(Key::space));
    REQUIRE_FALSE(controller.simulation().running());
    frame(controller, camera, on_button);
    CHECK_MESSAGE(controller.selected_mover() == 0, "after the start, a click there is the floor's again");
    CHECK_FALSE(controller.simulation().running());
}

TEST_CASE("a running blueprint advances by the frame time and a paused one stands still") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, {}, 0.1);
    CHECK(controller.simulation().ticks() == 0);
    frame(controller, camera, key(Key::space));
    frame(controller, camera, {}, 0.1);
    CHECK_MESSAGE(controller.simulation().ticks() == 20, "0.1 s at 200 updates per second");
}

TEST_CASE("an invalid frame time is rejected before any input is applied") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    auto input = click(camera, mover_position(controller, 0));
    input.pressed[static_cast<std::size_t>(Key::space)] = true;
    CHECK_THROWS_AS(controller.handle_input(-1, input, camera, {}), std::invalid_argument);
    CHECK_THROWS_AS(controller.handle_input(std::nan(""), input, camera, {}), std::invalid_argument);
    CHECK(controller.simulation().state() == RunState::paused);
    CHECK_FALSE(controller.selected_mover());
}

TEST_CASE("g toggles the workload") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    CHECK(controller.simulation().workload());
    frame(controller, camera, key(Key::g));
    CHECK_FALSE(controller.simulation().workload());
    frame(controller, camera, key(Key::g));
    CHECK(controller.simulation().workload());
}

TEST_CASE("a session starts with every layer and part but the heatmap and contact shapes") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    const auto &layers = controller.layers();
    for (std::size_t i = 0; i < i4sim::layer_count; ++i) {
        const auto layer = static_cast<Layer>(i);
        CAPTURE(i);
        const bool off = layer == Layer::heatmap || layer == Layer::contact_shapes;
        CHECK(layers.selected(layer) == !off);
        CHECK(layers.shows(layer) == !off);
    }
}

TEST_CASE("the keys one to six toggle their layers") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    const auto &layers = controller.layers();
    frame(controller, camera, key(Key::one));
    CHECK_FALSE(layers.selected(Layer::grid));
    frame(controller, camera, key(Key::two));
    CHECK_MESSAGE(layers.selected(Layer::heatmap), "key two turns on the heatmap, which starts off");
    frame(controller, camera, key(Key::three));
    CHECK_FALSE(layers.selected(Layer::paths));
    frame(controller, camera, key(Key::four));
    CHECK_FALSE(layers.selected(Layer::forces));
    frame(controller, camera, key(Key::five));
    CHECK_FALSE(layers.selected(Layer::borders));
    frame(controller, camera, key(Key::six));
    CHECK_FALSE(layers.selected(Layer::detection));
    CHECK_FALSE_MESSAGE(layers.selected(Layer::contact_shapes),
                        "contact shapes have a menu entry only, so no key turned them on");
    frame(controller, camera, key(Key::one));
    CHECK(layers.selected(Layer::grid));
}

TEST_CASE("q and e halve and double the update rate within its bounds") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, key(Key::e));
    CHECK(controller.simulation().updates_per_second() == 400);
    frame(controller, camera, key(Key::q));
    frame(controller, camera, key(Key::q));
    CHECK(controller.simulation().updates_per_second() == 100);
    for (int i = 0; i < 10; ++i)
        frame(controller, camera, key(Key::q));
    CHECK(controller.simulation().updates_per_second() == 1);
    for (int i = 0; i < 20; ++i)
        frame(controller, camera, key(Key::e));
    CHECK(controller.simulation().updates_per_second() == 10000);
}

TEST_CASE("r restarts a blueprint paused, awaiting its start, with its update rate and motion settings") {
    FactoryController controller(42, true, 360);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    choose_value(controller, "PROFILE", "ORIGINAL");
    step(controller, "SPEED");
    frame(controller, camera, click(camera, mover_position(controller, 0)));
    REQUIRE(controller.selected_mover() == 0);
    frame(controller, camera, {}, 0.5);
    REQUIRE(controller.simulation().ticks() == 180);

    frame(controller, camera, key(Key::r));
    const auto &simulation = controller.simulation();
    CHECK(simulation.ticks() == 0);
    CHECK(simulation.state() == RunState::paused);
    CHECK_MESSAGE(controller.awaiting_start(), "R shows the RUN button again");
    CHECK(simulation.updates_per_second() == 360);
    CHECK(simulation.motion_settings().profile == i4sim::SteeringProfile::original);
    CHECK(simulation.motion_settings().speed == 2.5f);
    CHECK_FALSE(controller.selected_mover());
    CHECK(controller.message().empty());

    frame(controller, camera, key(Key::e));
    REQUIRE(controller.simulation().updates_per_second() == 720);
    frame(controller, camera, key(Key::r));
    CHECK(controller.simulation().updates_per_second() == 720);
}

TEST_CASE("r replays a scene's goals with the motion settings in use and keeps it running") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(i4sim::test::showcase_blueprint("corridor-passing")));
    auto camera = camera_for(controller);
    const Vec2 start = mover_position(controller, 0);
    choose_value(controller, "PLANNER", "A*");
    REQUIRE_FALSE(controller.simulation().motion_settings().heatmap);
    frame(controller, camera, {}, 0.5);
    REQUIRE(mover_position(controller, 0).x != start.x);

    frame(controller, camera, key(Key::r));
    const auto &simulation = controller.simulation();
    CHECK(simulation.ticks() == 0);
    CHECK(mover_position(controller, 0).x == start.x);
    CHECK_FALSE(simulation.motion_settings().heatmap);
    CHECK(simulation.motion_settings().corridor_traffic);
    CHECK_FALSE(simulation.workload());
    for (const auto &mover : simulation.movers())
        CHECK_MESSAGE(mover.moving(), "the scene's goals are sent again");
    CHECK_FALSE(controller.description().empty());
    CHECK(simulation.running());
    CHECK_FALSE(controller.awaiting_start());

    frame(controller, camera, key(Key::space));
    frame(controller, camera, key(Key::r));
    CHECK_MESSAGE(controller.simulation().running(), "R plays a scene also when it was paused");
}

TEST_CASE("a restarting scene starts again by itself and keeps its rate and run state") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(i4sim::test::showcase_blueprint("corridor-recovery")));
    auto camera = camera_for(controller);
    frame(controller, camera, key(Key::e)); // 400 ticks per second
    const Vec2 start = mover_position(controller, 0);
    const auto settled = [&] {
        for (const auto &mover : controller.simulation().movers())
            if (mover.lifecycle != i4sim::MoverLifecycle::idle)
                return false;
        return true;
    };
    // A frame of 0.01 s runs four ticks, so the restart comes within a frame of the pause.
    std::uint64_t ticks_at_settle = 0;
    for (int f = 0; f < 10000 && !ticks_at_settle; ++f) {
        frame(controller, camera, {}, 0.01);
        if (settled())
            ticks_at_settle = controller.simulation().ticks();
    }
    REQUIRE_MESSAGE(ticks_at_settle > 0, "The corridor-recovery scene did not settle");
    REQUIRE(mover_position(controller, 0).x != start.x);
    // The pause is 400 ticks, 100 frames; give it twice that before calling it missing.
    for (int f = 0; f < 200 && controller.simulation().ticks() >= ticks_at_settle; ++f)
        frame(controller, camera, {}, 0.01);
    REQUIRE_MESSAGE(controller.simulation().ticks() < ticks_at_settle, "The scene did not restart");
    const auto &simulation = controller.simulation();
    CHECK_MESSAGE(simulation.ticks() <= 4, "The scene did not start again from tick zero");
    CHECK(simulation.running());
    CHECK(simulation.updates_per_second() == 400);
    CHECK(simulation.motion_settings().corridor_traffic);
    for (const auto &mover : simulation.movers())
        CHECK_MESSAGE(mover.moving(), "the restarted scene sends its goals again");
}

TEST_CASE("a paused restarting scene stepped past its pause restarts paused without awaiting its start") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(i4sim::test::showcase_blueprint("corridor-recovery")));
    REQUIRE(controller.simulation().running());
    auto camera = camera_for(controller);
    const auto settled = [&] {
        for (const auto &mover : controller.simulation().movers())
            if (mover.lifecycle != i4sim::MoverLifecycle::idle)
                return false;
        return true;
    };
    for (int f = 0; f < 10000 && !settled(); ++f)
        frame(controller, camera, {}, 0.05);
    REQUIRE_MESSAGE(settled(), "The corridor-recovery scene did not settle");
    frame(controller, camera, key(Key::space));
    REQUIRE_FALSE(controller.simulation().running());
    REQUIRE_FALSE(controller.awaiting_start());

    // SINGLE TICK advances the pause tick by tick; the frame after it lets the scene restart.
    const auto ticks_before = controller.simulation().ticks();
    for (int i = 0; i < 1000 && controller.simulation().ticks() >= ticks_before; ++i) {
        choose(controller, "SINGLE TICK");
        frame(controller, camera);
    }
    REQUIRE_MESSAGE(controller.simulation().ticks() < ticks_before, "The paused scene did not restart");
    CHECK_FALSE(controller.simulation().running());
    CHECK_MESSAGE(!controller.awaiting_start(), "a scheduled replay is not a start to wait for");
}

TEST_CASE("a shuttling scene does not restart") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(i4sim::test::showcase_blueprint("junction-crossing")));
    auto camera = camera_for(controller);
    for (int f = 0; f < 50; ++f)
        frame(controller, camera, {}, 0.5);
    CHECK_MESSAGE(controller.simulation().ticks() == 5000, "A shuttling scene was restarted");
}

TEST_CASE("a mover stays selected until the inspector closes or selects another mover") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, click(camera, mover_position(controller, 2)));
    CHECK(controller.selected_mover() == 2);
    CHECK_FALSE(controller.selected_producer());

    // Producer 0 is the Kuka at (360, 120); its symbol has a radius of half a cell, 120 mm.
    frame(controller, camera, click(camera, {360 + 100, 120}));
    CHECK(controller.selected_producer() == 0);
    CHECK(controller.selected_mover() == 2);
    CHECK_FALSE(controller.simulation().producers()[0].blocked);

    frame(controller, camera, click(camera, {1900, 600}));
    CHECK(controller.selected_mover() == 2);
    CHECK_FALSE(controller.selected_producer());
    frame(controller, camera, click(camera, mover_position(controller, 2)));
    CHECK(controller.selected_mover() == 2);
    frame(controller, camera, click(camera, mover_position(controller, 1)));
    CHECK(controller.selected_mover() == 1);
    controller.clear_selected_mover();
    CHECK_FALSE(controller.selected_mover());
}

TEST_CASE("a right click holds a mover or blocks a producer and keeps the selection on empty floor") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, click(camera, mover_position(controller, 1), true));
    CHECK(controller.selected_mover() == 1);
    CHECK(controller.simulation().movers()[1].held);
    frame(controller, camera, click(camera, {1900, 600}, true));
    CHECK_MESSAGE(controller.selected_mover() == 1, "a right click on empty floor keeps the selection");
    frame(controller, camera, click(camera, mover_position(controller, 1), true));
    CHECK_FALSE(controller.simulation().movers()[1].held);

    frame(controller, camera, click(camera, {360, 120}, true));
    CHECK(controller.selected_producer() == 0);
    CHECK(controller.simulation().producers()[0].blocked);
}

TEST_CASE("a shift right click selects like a left click and holds or blocks nothing") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    // Dotio inspects with Shift+right-click, so the same gesture must not stop a mover here.
    const auto shift_right_click = [&camera](Vec2 point) {
        auto input = click(camera, point, true);
        input.held[static_cast<std::size_t>(Key::shift)] = true;
        return input;
    };
    frame(controller, camera, shift_right_click(mover_position(controller, 1)));
    CHECK(controller.selected_mover() == 1);
    CHECK_FALSE(controller.simulation().movers()[1].held);

    frame(controller, camera, shift_right_click({360, 120}));
    CHECK(controller.selected_producer() == 0);
    CHECK_FALSE(controller.simulation().producers()[0].blocked);
    CHECK_MESSAGE(controller.selected_mover() == 1, "selecting a producer keeps the mover");

    frame(controller, camera, shift_right_click({1900, 600}));
    CHECK_MESSAGE(!controller.selected_producer(),
                  "on empty floor it clears the producer, as a left click does");
    CHECK(controller.selected_mover() == 1);
    CHECK_MESSAGE(controller.simulation().movers()[1].goal != i4sim::GoalKind::manual,
                  "a goal needs a left click");
}

TEST_CASE("shift-click sends the selected mover and a rejected goal shows a message for four seconds") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(open_floor));
    auto camera = camera_for(controller);
    frame(controller, camera, click(camera, mover_position(controller, 0)));
    REQUIRE(controller.selected_mover() == 0);
    auto goal = click(camera, {1200, 400});
    goal.held[static_cast<std::size_t>(Key::shift)] = true;
    frame(controller, camera, goal);
    const auto &west = controller.simulation().movers()[0];
    CHECK(west.destination.x == doctest::Approx(1200));
    CHECK(west.destination.y == doctest::Approx(400));
    CHECK(west.goal == i4sim::GoalKind::manual);
    CHECK(controller.message().empty());
    CHECK_MESSAGE(controller.selected_mover() == 0, "a goal keeps the selection");

    frame(controller, camera, click(camera, mover_position(controller, 0), true));
    REQUIRE(west.held);
    auto rejected = click(camera, {1200, 1200});
    rejected.held[static_cast<std::size_t>(Key::shift)] = true;
    frame(controller, camera, rejected);
    CHECK(controller.message().find("GOAL REJECTED") != std::string::npos);
    CHECK(west.destination.y == doctest::Approx(400));
    frame(controller, camera, {}, 3.9);
    CHECK_FALSE(controller.message().empty());
    frame(controller, camera, {}, 0.2);
    CHECK_MESSAGE(controller.message().empty(), "the message expires after four seconds");
}

TEST_CASE("a captured pointer selects nothing and does not zoom") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    const float zoom = camera.pixels_per_unit;
    auto input = click(camera, mover_position(controller, 0));
    input.wheel = 2;
    frame(controller, camera, input, 0, {true, true});
    CHECK_FALSE(controller.selected_mover());
    CHECK(camera.pixels_per_unit == zoom);
    frame(controller, camera, key(Key::space), 0, {true, true});
    CHECK_MESSAGE(controller.simulation().running(), "keys still work while the pointer is on a panel");
}

TEST_CASE("the camera pans with frame time and zoom, and the wheel zooms in debug mode only") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    camera.pixels_per_unit = 2;
    const Vec2 start = camera.position;
    Input right;
    right.held[static_cast<std::size_t>(Key::d)] = true;
    frame(controller, camera, right, 0.05);
    CHECK_MESSAGE(camera.position.x - start.x == doctest::Approx(10),
                  "0.05 s at 400 pixels per second, 2 pixels per mm");
    CHECK(camera.position.y == start.y);
    Input up;
    up.held[static_cast<std::size_t>(Key::up)] = true;
    frame(controller, camera, up, 5);
    CHECK_MESSAGE(camera.position.y - start.y == doctest::Approx(20), "a long frame pans as 0.1 s");

    Input wheel;
    wheel.wheel = 1;
    frame(controller, camera, wheel, 0, {false, false});
    CHECK_MESSAGE(camera.pixels_per_unit == 2, "outside debug mode the wheel does not zoom");
    frame(controller, camera, wheel);
    CHECK(camera.pixels_per_unit == doctest::Approx(2.2));
    camera.pixels_per_unit = 500;
    frame(controller, camera);
    CHECK_MESSAGE(camera.pixels_per_unit == i4sim::zoom_limits.maximum, "every frame clamps the zoom");
}

TEST_CASE("f and the frame scene entry fit the camera again") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    const auto fitted = camera;
    camera.position = {0, 0};
    camera.pixels_per_unit = 3;
    frame(controller, camera, key(Key::f));
    CHECK(camera.position.x == fitted.position.x);
    CHECK(camera.pixels_per_unit == fitted.pixels_per_unit);

    camera.position = {0, 0};
    choose(controller, "FRAME SCENE");
    frame(controller, camera);
    CHECK_MESSAGE(camera.position == fitted.position,
                  "Camera position (" << camera.position.x << ", " << camera.position.y << ") expected ("
                                      << fitted.position.x << ", " << fitted.position.y << ")");
}

TEST_CASE("an open panel pauses a running blueprint and restores it afterwards") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(factory));
    controller.suspend(false);
    CHECK(controller.simulation().running());
    controller.suspend(true);
    CHECK_FALSE(controller.simulation().running());
    controller.suspend(true);
    CHECK_FALSE(controller.simulation().running());
    controller.suspend(false);
    CHECK(controller.simulation().running());
}

TEST_CASE("an open panel leaves a paused blueprint paused") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    controller.suspend(true);
    controller.suspend(false);
    CHECK(controller.simulation().state() == RunState::paused);
}

TEST_CASE("the menu starts with the simulation section open") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    CHECK(labels(controller) == std::vector<std::string>{"SIMULATION", "RUNNING", "SINGLE TICK", "UPS",
                                                         "WORKLOAD", "LAYERS", "MOTION", "TRAFFIC", "ZOOM",
                                                         "FRAME SCENE"});
    const auto menu = controller.menu();
    CHECK((menu[0].kind == kinetra::DebugEntryKind::group && menu[0].active() && menu[0].depth == 0));
    CHECK_FALSE(menu[5].active());
    CHECK(menu[1].depth == 1);
    CHECK_MESSAGE((menu[8].label == "ZOOM" && menu[8].depth == 0 && menu[8].rule_above),
                  "a rule sets the commands apart from the sections");
    CHECK_MESSAGE(menu[9].depth == 0, "FRAME SCENE stays outside the sections");
}

TEST_CASE("a section heading opens its rows and closes the open section") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    choose(controller, "TRAFFIC");
    CHECK(labels(controller) == std::vector<std::string>{"SIMULATION", "LAYERS", "MOTION", "TRAFFIC",
                                                         "CORRIDOR TRAFFIC", "JUNCTION TRAFFIC",
                                                         "PARKING TRAFFIC", "PREDICTIVE PASS", "MAKE WAY",
                                                         "DEPARTURE CHECK", "ZOOM", "FRAME SCENE"});
    choose(controller, "TRAFFIC");
    CHECK(labels(controller) ==
          std::vector<std::string>{"SIMULATION", "LAYERS", "MOTION", "TRAFFIC", "ZOOM", "FRAME SCENE"});
}

TEST_CASE("the layers section lists every layer as a check box, each with its parts") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "LAYERS");
    CHECK(labels(controller) ==
          std::vector<std::string>{
              "SIMULATION",      "LAYERS",          "GRID",         "HEATMAP", "PATHS",     "ROUTES",
              "RETREAT TARGETS", "PASSING TARGETS", "FORCES",       "SEEK",    "COLLISION", "BORDER",
              "PREDICTIVE",      "VELOCITY",        "ACCELERATION", "BORDERS", "WALLS",     "TRAFFIC ZONES",
              "DETECTION",       "CONTACT SHAPES",  "MOTION",       "TRAFFIC", "ZOOM",      "FRAME SCENE"});
    const auto paths = entry(controller, "PATHS");
    CHECK((paths.kind == kinetra::DebugEntryKind::check && paths.depth == 1));
    const auto routes = entry(controller, "ROUTES");
    CHECK((routes.kind == kinetra::DebugEntryKind::check && routes.depth == 2));
    CHECK_MESSAGE(!routes.swatch, "without colors no row has a swatch");
}

TEST_CASE("a layer row gets the swatch the drawing code gives for its layer") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "LAYERS");
    // Stand-in for layer_color: the layer's number as red, and no color for PATHS.
    const auto menu = controller.menu([](Layer layer) -> std::optional<kinetra::Color> {
        if (layer == Layer::paths)
            return std::nullopt;
        return kinetra::Color{float(layer), 0, 0};
    });
    for (const auto &row : menu) {
        if (row.label == "ROUTES") {
            REQUIRE(row.swatch);
            CHECK(row.swatch->r == float(Layer::routes));
        }
        if (row.label == "CONTACT SHAPES") {
            REQUIRE(row.swatch);
            CHECK(row.swatch->r == float(Layer::contact_shapes));
        }
        if (row.label == "PATHS" || row.label == "LAYERS" || row.label == "ZOOM")
            CHECK_MESSAGE(!row.swatch, row.label << " has a swatch");
    }
}

TEST_CASE("the zoom row zooms the camera by one step per click within the limits") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera);
    const float fitted = camera.pixels_per_unit;
    CHECK(entry(controller, "ZOOM").value() == kinetra::debug_number_text(fitted));
    entry(controller, "ZOOM").increase();
    CHECK_MESSAGE(camera.pixels_per_unit == fitted, "the row acts with the frame's camera update");
    frame(controller, camera);
    CHECK(camera.pixels_per_unit == doctest::Approx(fitted * kinetra::zoom_factor));
    entry(controller, "ZOOM").decrease();
    entry(controller, "ZOOM").decrease();
    frame(controller, camera);
    CHECK(camera.pixels_per_unit == doctest::Approx(fitted / kinetra::zoom_factor));
    for (int i = 0; i < 200; ++i)
        entry(controller, "ZOOM").decrease();
    frame(controller, camera);
    CHECK(camera.pixels_per_unit == i4sim::zoom_limits.minimum);
}

TEST_CASE("a layer lists its parts only while it is on") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "LAYERS");
    auto camera = camera_for(controller);
    frame(controller, camera, key(Key::four)); // FORCES off
    choose(controller, "BORDERS");
    const auto rows = labels(controller);
    for (const char *part : {"SEEK", "ACCELERATION", "WALLS", "TRAFFIC ZONES"})
        CHECK_MESSAGE(std::find(rows.begin(), rows.end(), part) == rows.end(), part << " is listed");
    CHECK_MESSAGE(std::find(rows.begin(), rows.end(), "ROUTES") != rows.end(), "PATHS is still on");
    choose(controller, "FORCES");
    CHECK(entry(controller, "SEEK").active());
}

TEST_CASE("a part is drawn only while it and its layer are on") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "LAYERS");
    const auto &layers = controller.layers();
    choose(controller, "ROUTES");
    CHECK_FALSE(layers.shows(Layer::routes));
    CHECK(layers.shows(Layer::retreat_targets));
    CHECK_MESSAGE(layers.shows(Layer::paths), "turning a part off keeps its layer");
    choose(controller, "ROUTES");
    auto camera = camera_for(controller);
    frame(controller, camera, key(Key::three)); // PATHS off with the key
    CHECK_FALSE(layers.shows(Layer::routes));
    CHECK_MESSAGE(layers.selected(Layer::routes), "the part keeps its selection for when PATHS returns");
    frame(controller, camera, key(Key::three));
    CHECK(layers.shows(Layer::routes));
    CHECK_FALSE(i4sim::ViewLayers::none().shows(Layer::routes));
}

TEST_CASE("the motion section lists the profile, the planner and the parameters") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "MOTION");
    CHECK(labels(controller) == std::vector<std::string>{"SIMULATION", "LAYERS", "MOTION", "PROFILE",
                                                         "PLANNER", "RECOVERY", "SPEED", "FORCE", "HORIZON",
                                                         "RESET PARAMETERS", "TRAFFIC", "ZOOM",
                                                         "FRAME SCENE"});
    CHECK_MESSAGE(!entry(controller, "SPEED").action, "a number row changes only with its buttons");
}

TEST_CASE("the workload entry is a check box on the simulation's workload") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    REQUIRE(entry(controller, "WORKLOAD").kind == kinetra::DebugEntryKind::check);
    CHECK(entry(controller, "WORKLOAD").active());
    choose(controller, "WORKLOAD");
    CHECK_FALSE(controller.simulation().workload());
    CHECK_FALSE(entry(controller, "WORKLOAD").active());
    auto camera = camera_for(controller);
    frame(controller, camera, key(Key::g));
    CHECK_MESSAGE(entry(controller, "WORKLOAD").active(), "the box follows the G key");
}

TEST_CASE("the layer entries are check boxes on the layers") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "LAYERS");
    const auto &layers = controller.layers();
    for (const char *label : {"GRID", "HEATMAP", "PATHS", "FORCES", "BORDERS", "DETECTION", "CONTACT SHAPES"})
        choose(controller, label);
    for (auto layer : top_layers) {
        CAPTURE(static_cast<int>(layer));
        // HEATMAP and CONTACT SHAPES start off, so the click turned them on.
        CHECK(layers.selected(layer) == (layer == Layer::heatmap || layer == Layer::contact_shapes));
    }
    CHECK(entry(controller, "CONTACT SHAPES").active());
    choose(controller, "CONTACT SHAPES");
    CHECK_FALSE(layers.selected(Layer::contact_shapes));
    CHECK_FALSE(entry(controller, "CONTACT SHAPES").active());
    CHECK_MESSAGE(entry(controller, "FRAME SCENE").kind == kinetra::DebugEntryKind::button,
                  "a command is a button, not a check box");
}

TEST_CASE("the run entries pause, resume and step a single tick") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    choose(controller, "SINGLE TICK");
    CHECK(controller.simulation().ticks() == 1);
    CHECK(controller.simulation().state() == RunState::paused);
    CHECK_MESSAGE(controller.awaiting_start(), "a single tick is not a start");
    CHECK_FALSE(entry(controller, "RUNNING").active());
    choose(controller, "RUNNING");
    CHECK(controller.simulation().state() == RunState::running);
    CHECK_FALSE(controller.awaiting_start());
    CHECK(entry(controller, "RUNNING").active());
    choose(controller, "RUNNING");
    CHECK(controller.simulation().state() == RunState::paused);
}

TEST_CASE("the profile list shows the four profiles and closes on a choice") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "MOTION");
    CHECK(entry(controller, "PROFILE").value() == "IMPROVED");
    CHECK_FALSE(entry(controller, "PROFILE").active());
    choose(controller, "PROFILE");
    CHECK(entry(controller, "PROFILE").active());
    const auto menu = controller.menu();
    std::vector<std::string> values;
    for (const auto &row : menu)
        if (row.kind == kinetra::DebugEntryKind::choice) {
            values.emplace_back(row.label);
            CHECK(row.depth == 2);
            CHECK(row.active() == (row.label == "IMPROVED"));
        }
    CHECK(values == std::vector<std::string>{"IMPROVED", "ORIGINAL", "RVO EXPERIMENT", "RECIPROCAL"});
    choose(controller, "RECIPROCAL");
    CHECK(controller.simulation().motion_settings().profile == i4sim::SteeringProfile::reciprocal);
    CHECK(entry(controller, "PROFILE").value() == "RECIPROCAL");
    CHECK_MESSAGE(!entry(controller, "PROFILE").active(), "choosing a value closes the list");
    CHECK(labels(controller).size() == 13);
}

TEST_CASE("choosing the current value only closes the list") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    // A motion change would replan every route and clear the message; the message shows that
    // no change ran.
    controller.report_error("KEEP");
    choose_value(controller, "PROFILE", "IMPROVED");
    choose_value(controller, "PLANNER", "HEAT A*");
    CHECK(controller.message() == "KEEP");
    CHECK_FALSE(entry(controller, "PLANNER").active());
    choose_value(controller, "PLANNER", "A*");
    CHECK(controller.message().empty());
}

TEST_CASE("the planner list chooses between heat A*, A* and no path planning") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "MOTION");
    const auto motion = [&]() -> const i4sim::MotionSettings & {
        return controller.simulation().motion_settings();
    };
    CHECK(entry(controller, "PLANNER").value() == "HEAT A*");
    choose_value(controller, "PLANNER", "A*");
    CHECK_FALSE(motion().heatmap);
    CHECK(motion().path_planning);
    CHECK(entry(controller, "PLANNER").value() == "A*");
    choose_value(controller, "PLANNER", "OFF");
    CHECK_FALSE(motion().path_planning);
    CHECK_MESSAGE(!motion().heatmap, "OFF keeps the heatmap setting for when planning returns");
    CHECK(entry(controller, "PLANNER").value() == "OFF");
    choose_value(controller, "PLANNER", "HEAT A*");
    CHECK(motion().path_planning);
    CHECK(motion().heatmap);
}

TEST_CASE("a list closes with its section") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "MOTION");
    choose(controller, "PROFILE");
    open_section(controller, "TRAFFIC");
    open_section(controller, "MOTION");
    CHECK_FALSE(entry(controller, "PROFILE").active());
}

TEST_CASE("the number rows show their value and step it while the blueprint runs") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "MOTION");
    const auto &motion = controller.simulation().motion_settings();
    CHECK(entry(controller, "SPEED").value() == "2");
    step(controller, "SPEED");
    CHECK(motion.speed == 2.5f);
    CHECK(entry(controller, "SPEED").value() == "2.5");
    step(controller, "SPEED", false);
    CHECK(motion.speed == 2);
    CHECK(entry(controller, "FORCE").value() == "0.6");
    step(controller, "FORCE");
    CHECK(motion.force == doctest::Approx(0.75));
    step(controller, "FORCE", false);
    CHECK(motion.force == doctest::Approx(0.6));
    CHECK(entry(controller, "HORIZON").value() == "32");
    step(controller, "HORIZON", false);
    CHECK(motion.horizon == 31);
    for (int i = 0; i < 40; ++i)
        step(controller, "HORIZON");
    CHECK_MESSAGE(motion.horizon == 64, "the horizon stops at its limit");
    open_section(controller, "SIMULATION");
    CHECK(entry(controller, "UPS").value() == "200");
    step(controller, "UPS");
    CHECK(controller.simulation().updates_per_second() == 400);
    CHECK(entry(controller, "UPS").value() == "400");
    step(controller, "UPS", false);
    CHECK(controller.simulation().updates_per_second() == 200);
    CHECK(controller.simulation().running());
    CHECK(controller.message().empty());
}

TEST_CASE("reset parameters restores the default speed, force and horizon") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "MOTION");
    for (const char *label : {"SPEED", "FORCE", "HORIZON"})
        step(controller, label);

    choose(controller, "RESET PARAMETERS");
    const auto &motion = controller.simulation().motion_settings();
    const i4sim::MotionSettings defaults;
    CHECK(motion.speed == defaults.speed);
    CHECK(motion.force == defaults.force);
    CHECK(motion.horizon == defaults.horizon);
}

TEST_CASE("the recovery and traffic entries are check boxes on the motion settings") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    const auto &motion = controller.simulation().motion_settings();
    open_section(controller, "TRAFFIC");
    for (const char *label : {"CORRIDOR TRAFFIC", "JUNCTION TRAFFIC", "PARKING TRAFFIC", "PREDICTIVE PASS",
                              "MAKE WAY", "DEPARTURE CHECK"}) {
        CHECK(entry(controller, label).kind == kinetra::DebugEntryKind::check);
        CHECK_FALSE(entry(controller, label).active());
        choose(controller, label);
        CHECK(entry(controller, label).active());
    }
    CHECK((motion.corridor_traffic && motion.junction_traffic && motion.parking_traffic &&
           motion.predictive_passing && motion.make_way && motion.departure_check));
    CHECK_FALSE(entry(controller, "PREDICTIVE PASS").value);

    open_section(controller, "MOTION");
    CHECK_FALSE(entry(controller, "RECOVERY").active());
    choose(controller, "RECOVERY");
    CHECK(motion.recovery);
    CHECK(entry(controller, "RECOVERY").active());
    choose_value(controller, "PROFILE", "ORIGINAL"); // Original has no predictive passing.
    open_section(controller, "TRAFFIC");
    CHECK(entry(controller, "PREDICTIVE PASS").value() == "LEGACY");
    CHECK_MESSAGE(entry(controller, "PREDICTIVE PASS").active(), "the legacy profile keeps the setting");
}

TEST_CASE("a motion setting the simulation rejects shows its reason and changes nothing") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    open_section(controller, "MOTION");
    // Speed grows by a quarter per step until it leaves the range of a float.
    int steps = 0;
    while (controller.message().empty() && steps < 1000) {
        step(controller, "SPEED");
        ++steps;
    }
    REQUIRE_FALSE(controller.message().empty());
    CHECK(std::isfinite(controller.simulation().motion_settings().speed));
    REQUIRE(controller.begin_frame(4));
    CHECK_MESSAGE(controller.message().empty(), "the reason expires like a rejected goal");
}

TEST_CASE("close_later closes the blueprint at the next frame") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, click(camera, mover_position(controller, 0)));
    controller.suspend(true);
    controller.close_later();
    CHECK_MESSAGE(controller.loaded(), "the frame that asked keeps its simulation");
    CHECK_FALSE(controller.begin_frame(0));
    CHECK_FALSE(controller.loaded());
    CHECK_FALSE(controller.selected_mover());

    REQUIRE(controller.load(factory));
    controller.suspend(false);
    CHECK_MESSAGE(controller.simulation().running(), "the next blueprint does not inherit the old pause");
    CHECK(controller.begin_frame(0));

    controller.close_later();
    REQUIRE(controller.load(open_floor));
    CHECK_MESSAGE(controller.begin_frame(0), "a load drops the close requested for the old blueprint");
    CHECK(controller.simulation().blueprint().name == "Predictive passing");
}
