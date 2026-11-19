// Window-free tests for the i4sim session's controller (labs/i4sim/include/controller.h):
// loading, keys, pointer, camera, modal pause and the debug menu's entries. The blueprints
// come from the source tree.
#include "controller.h"
#include "showcase.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace kinetra;
using i4sim::FactoryController;
using i4sim::flip_y;
using i4sim::MenuEntry;
using i4sim::PanelState;
using i4sim::RunState;

namespace {
const auto factory = i4sim::test::original_blueprint("MAS-demonstrator");
/// An open floor of 2400 by 1600 mm with two movers at (500, 800) and (1900, 800).
const auto open_floor = i4sim::test::showcase_blueprint("predictive-passing");

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

MenuEntry entry(FactoryController &controller, std::string_view label) {
    for (auto &candidate : controller.menu())
        if (candidate.label == label)
            return candidate;
    FAIL("The menu page has no entry " << label);
    return {};
}

/// Runs the entry `label` of the current page, as a click on its row does.
void choose(FactoryController &controller, std::string_view label) {
    entry(controller, label).action();
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
    CHECK(simulation.blueprint().name == "MAS-demonstrator");
    CHECK(simulation.state() == RunState::paused);
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
    CHECK(camera.position.x == floor.x / 2);
    CHECK(camera.position.y == -floor.y / 2);
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
    CHECK(controller.simulation().blueprint().name == "MAS-demonstrator");
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
    CHECK_MESSAGE(simulation.state() == RunState::paused, "a scene waits for Space");
}

TEST_CASE("space and p pause and resume") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, key(Key::space));
    CHECK(controller.simulation().state() == RunState::running);
    frame(controller, camera, key(Key::p));
    CHECK(controller.simulation().state() == RunState::paused);
    frame(controller, camera);
    CHECK_MESSAGE(controller.simulation().state() == RunState::paused,
                  "a frame without the key changes nothing");
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

TEST_CASE("the keys one to six toggle their layers") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    const auto &layers = controller.layers();
    CHECK((layers.grid && layers.heatmap && layers.paths && layers.forces && layers.borders &&
           layers.detection && layers.contact_shapes));
    frame(controller, camera, key(Key::one));
    CHECK_FALSE(layers.grid);
    frame(controller, camera, key(Key::two));
    CHECK_FALSE(layers.heatmap);
    frame(controller, camera, key(Key::three));
    CHECK_FALSE(layers.paths);
    frame(controller, camera, key(Key::four));
    CHECK_FALSE(layers.forces);
    frame(controller, camera, key(Key::five));
    CHECK_FALSE(layers.borders);
    frame(controller, camera, key(Key::six));
    CHECK_FALSE(layers.detection);
    CHECK_MESSAGE(layers.contact_shapes, "contact shapes have a menu entry only");
    frame(controller, camera, key(Key::one));
    CHECK(layers.grid);
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

TEST_CASE("r restarts the blueprint paused with the motion settings in use") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    choose(controller, "PAGE: VIEW");
    choose(controller, "PROFILE");
    choose(controller, "SPEED +");
    frame(controller, camera, click(camera, mover_position(controller, 0)));
    REQUIRE(controller.selected_mover() == 0);
    frame(controller, camera, {}, 0.5);
    REQUIRE(controller.simulation().ticks() == 100);

    frame(controller, camera, key(Key::r));
    const auto &simulation = controller.simulation();
    CHECK(simulation.ticks() == 0);
    CHECK(simulation.state() == RunState::paused);
    CHECK(simulation.motion_settings().profile == i4sim::SteeringProfile::original);
    CHECK(simulation.motion_settings().speed == 2.5f);
    CHECK_FALSE(controller.selected_mover());
    CHECK(controller.message().empty());
}

TEST_CASE("r replays a scene's goals with the motion settings in use") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(i4sim::test::showcase_blueprint("corridor-passing")));
    auto camera = camera_for(controller);
    const Vec2 start = mover_position(controller, 0);
    choose(controller, "PAGE: VIEW");
    choose(controller, "PLANNER");
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
}

TEST_CASE("a click selects a mover or a producer and empty floor clears the selection") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, click(camera, mover_position(controller, 2)));
    CHECK(controller.selected_mover() == 2);
    CHECK_FALSE(controller.selected_producer());

    // Producer 0 is the Kuka at (360, 120); its symbol has a radius of half a cell, 120 mm.
    frame(controller, camera, click(camera, {360 + 100, 120}));
    CHECK(controller.selected_producer() == 0);
    CHECK_FALSE(controller.selected_mover());
    CHECK_FALSE(controller.simulation().producers()[0].blocked);

    frame(controller, camera, click(camera, {1900, 600}));
    CHECK_FALSE(controller.selected_mover());
    CHECK_FALSE(controller.selected_producer());
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
    CHECK(camera.position.x == fitted.position.x);
    CHECK(camera.position.y == fitted.position.y);
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

TEST_CASE("the view page lists the layers, the run controls and two traffic policies") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    CHECK(labels(controller) ==
          std::vector<std::string>{"PAGE: VIEW", "GRID", "HEATMAP", "PATHS", "FORCES", "BORDERS", "DETECTION",
                                   "CONTACT SHAPES", "PAUSE / RUN", "SINGLE TICK", "WORKLOAD", "FRAME SCENE",
                                   "BLUEPRINTS", "CORRIDOR TRAFFIC", "PREDICTIVE PASS"});
}

TEST_CASE("the motion page lists the profile, the parameters and two traffic policies") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    choose(controller, "PAGE: VIEW");
    CHECK(labels(controller) ==
          std::vector<std::string>{"PAGE: MOTION", "PROFILE", "PLANNER", "RECOVERY", "SPEED +", "SPEED -",
                                   "FORCE +", "FORCE -", "HORIZON +", "HORIZON -", "RESET PARAMETERS",
                                   "UPS +", "UPS -", "PARKING TRAFFIC", "JUNCTION TRAFFIC"});
    choose(controller, "PAGE: MOTION");
    CHECK(labels(controller).front() == "PAGE: VIEW");
}

TEST_CASE("the workload entry is a check box on the simulation's workload") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    REQUIRE(entry(controller, "WORKLOAD").checked);
    CHECK(entry(controller, "WORKLOAD").checked());
    choose(controller, "WORKLOAD");
    CHECK_FALSE(controller.simulation().workload());
    CHECK_FALSE(entry(controller, "WORKLOAD").checked());
    auto camera = camera_for(controller);
    frame(controller, camera, key(Key::g));
    CHECK_MESSAGE(entry(controller, "WORKLOAD").checked(), "the box follows the G key");
}

TEST_CASE("the layer entries are check boxes on the layers") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    const auto &layers = controller.layers();
    choose(controller, "GRID");
    choose(controller, "HEATMAP");
    choose(controller, "PATHS");
    choose(controller, "FORCES");
    choose(controller, "BORDERS");
    choose(controller, "DETECTION");
    choose(controller, "CONTACT SHAPES");
    CHECK_FALSE((layers.grid || layers.heatmap || layers.paths || layers.forces || layers.borders ||
                 layers.detection || layers.contact_shapes));
    CHECK_FALSE(entry(controller, "CONTACT SHAPES").checked());
    choose(controller, "CONTACT SHAPES");
    CHECK(layers.contact_shapes);
    CHECK(entry(controller, "CONTACT SHAPES").checked());
    CHECK_MESSAGE(!entry(controller, "PAUSE / RUN").checked, "a command is a button, not a check box");
}

TEST_CASE("the run entries pause, resume and step a single tick") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    choose(controller, "SINGLE TICK");
    CHECK(controller.simulation().ticks() == 1);
    CHECK(controller.simulation().state() == RunState::paused);
    choose(controller, "PAUSE / RUN");
    CHECK(controller.simulation().state() == RunState::running);
    choose(controller, "PAUSE / RUN");
    CHECK(controller.simulation().state() == RunState::paused);
}

TEST_CASE("the profile entry cycles the four profiles and names them") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    choose(controller, "PAGE: VIEW");
    std::vector<std::string> names;
    for (int i = 0; i < 4; ++i) {
        names.push_back(entry(controller, "PROFILE").value());
        choose(controller, "PROFILE");
    }
    CHECK(names == std::vector<std::string>{"IMPROVED", "ORIGINAL", "RVO EXPERIMENT", "RECIPROCAL"});
    CHECK(controller.simulation().motion_settings().profile == i4sim::SteeringProfile::improved);
}

TEST_CASE("motion entries apply while the blueprint runs and keep it running") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(factory));
    choose(controller, "PAGE: VIEW");
    const auto &motion = controller.simulation().motion_settings();
    choose(controller, "SPEED +");
    CHECK(motion.speed == 2.5f);
    choose(controller, "SPEED -");
    CHECK(motion.speed == 2);
    choose(controller, "FORCE +");
    CHECK(motion.force == doctest::Approx(0.75));
    choose(controller, "FORCE -");
    CHECK(motion.force == doctest::Approx(0.6));
    choose(controller, "HORIZON -");
    CHECK(motion.horizon == 31);
    for (int i = 0; i < 40; ++i)
        choose(controller, "HORIZON +");
    CHECK_MESSAGE(motion.horizon == 64, "the horizon stops at its limit");
    choose(controller, "RESET PARAMETERS");
    CHECK(motion.speed == 2);
    CHECK(motion.force == 0.6f);
    choose(controller, "UPS +");
    CHECK(controller.simulation().updates_per_second() == 400);
    choose(controller, "UPS -");
    CHECK(controller.simulation().updates_per_second() == 200);
    CHECK(controller.simulation().running());
    CHECK(controller.message().empty());
}

TEST_CASE("the switches of the planner and the traffic policies show their state") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    const auto &motion = controller.simulation().motion_settings();
    CHECK(entry(controller, "CORRIDOR TRAFFIC").value() == "OFF");
    choose(controller, "CORRIDOR TRAFFIC");
    CHECK(motion.corridor_traffic);
    CHECK(entry(controller, "CORRIDOR TRAFFIC").value() == "ON");
    CHECK(entry(controller, "PREDICTIVE PASS").value() == "OFF");
    choose(controller, "PREDICTIVE PASS");
    CHECK(motion.predictive_passing);
    CHECK(entry(controller, "PREDICTIVE PASS").value() == "ON");

    choose(controller, "PAGE: VIEW");
    CHECK(entry(controller, "PLANNER").value() == "HEAT A*");
    choose(controller, "PLANNER");
    CHECK(entry(controller, "PLANNER").value() == "A*");
    for (const char *label : {"RECOVERY", "PARKING TRAFFIC", "JUNCTION TRAFFIC"}) {
        CHECK(entry(controller, label).value() == "OFF");
        choose(controller, label);
        CHECK(entry(controller, label).value() == "ON");
    }
    CHECK((motion.recovery && motion.parking_traffic && motion.junction_traffic && !motion.heatmap));
    choose(controller, "PROFILE"); // Original has no predictive passing.
    choose(controller, "PAGE: MOTION");
    CHECK(entry(controller, "PREDICTIVE PASS").value() == "LEGACY");
}

TEST_CASE("a motion setting the simulation rejects shows its reason and changes nothing") {
    FactoryController controller(42, false, 200);
    REQUIRE(controller.load(factory));
    choose(controller, "PAGE: VIEW");
    // Speed grows by a quarter per step until it leaves the range of a float.
    int steps = 0;
    while (controller.message().empty() && steps < 1000) {
        choose(controller, "SPEED +");
        ++steps;
    }
    REQUIRE_FALSE(controller.message().empty());
    CHECK(std::isfinite(controller.simulation().motion_settings().speed));
    REQUIRE(controller.begin_frame(4));
    CHECK_MESSAGE(controller.message().empty(), "the reason expires like a rejected goal");
}

TEST_CASE("the blueprints entry closes the blueprint at the next frame") {
    FactoryController controller(42, true, 200);
    REQUIRE(controller.load(factory));
    auto camera = camera_for(controller);
    frame(controller, camera, click(camera, mover_position(controller, 0)));
    controller.suspend(true);
    choose(controller, "BLUEPRINTS");
    CHECK_MESSAGE(controller.loaded(), "the frame that chose the entry keeps its simulation");
    CHECK_FALSE(controller.begin_frame(0));
    CHECK_FALSE(controller.loaded());
    CHECK_FALSE(controller.selected_mover());

    REQUIRE(controller.load(factory));
    controller.suspend(false);
    CHECK_MESSAGE(controller.simulation().running(), "the next blueprint does not inherit the old pause");
    CHECK(controller.begin_frame(0));

    choose(controller, "BLUEPRINTS");
    REQUIRE(controller.load(open_floor));
    CHECK_MESSAGE(controller.begin_frame(0), "a load drops the close requested for the old blueprint");
    CHECK(controller.simulation().blueprint().name == "Predictive passing");
}
