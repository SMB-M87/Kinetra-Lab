// Headless tests for the Demo application layer: input commands, pause, time scale and
// single steps, camera, pointer actions, avoidance mode, tuning, player intent, drawn
// poses, and agents that stay around the player and clear of terrain, without a window.
#include "demo/application.h"
#include "demo/input/mapping.h"

#include <kinetra/simulation/avoidance.h>
#include <kinetra/simulation/steering.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace demo;
using namespace kinetra;

namespace {
Vec2 player_position(const Application &application) {
    const auto &scene = application.scene();
    return scene.world().body(scene.player()).position;
}

/// Elapsed time per frame in seconds; the Demo's fixed clock steps at the same rate.
constexpr double frame_seconds = 1.0 / 120;

/// The camera of a new Demo session in a 1280 by 800 window.
Camera2D session_camera() {
    Camera2D camera;
    camera.viewport = {1280, 800};
    camera.pixels_per_unit = initial_zoom;
    return camera;
}

/// Summed path length in world units that the active agents cover in `frames` frames.
float distance_covered(Application &application, Camera2D &camera, int frames) {
    float covered = 0;
    for (int frame = 0; frame < frames; ++frame) {
        std::vector<Vec2> before;
        for (const auto &agent : application.scene().agents())
            before.push_back(application.scene().world().body(agent.body_id()).position);
        application.advance(frame_seconds, {}, camera);
        // A standing player streams nothing in or out, so the agents keep their order.
        const auto agents = application.scene().agents();
        REQUIRE(agents.size() == before.size());
        for (std::size_t i = 0; i < agents.size(); ++i)
            covered += distance(application.scene().world().body(agents[i].body_id()).position, before[i]);
    }
    return covered;
}

/// The loaded terrain body of `shape` that lies nearest to the player of `scene`.
Body nearest_terrain(const Scene &scene, Shape shape) {
    const auto bodies = scene.world().bodies();
    const Vec2 player = bodies[scene.player()].position;
    std::optional<Body> nearest;
    for (std::size_t id = scene.moving_body_count(); id < bodies.size(); ++id)
        if (bodies[id].shape == shape &&
            (!nearest || distance(bodies[id].position, player) < distance(nearest->position, player)))
            nearest = bodies[id];
    REQUIRE_MESSAGE(nearest.has_value(), "The scene loaded no terrain of the wanted shape");
    return *nearest;
}
} // namespace

TEST_CASE("a steering wanderer heading for terrain gets away from it without touching it") {
    // Distance in world units between the agent and the obstacle's bounding circle at the start.
    constexpr float start_gap = 3;
    // Six simulated seconds: the agent reaches the obstacle in under one. Reciprocal
    // avoidance then slows it down in front of a wide box for up to three before it
    // finds the way around.
    constexpr int ticks = 720;
    for (const bool reciprocal : {true, false})
        for (const Shape shape : {Shape::circle, Shape::box}) {
            CAPTURE(reciprocal);
            CAPTURE(static_cast<int>(shape));
            Scene scene(1, 42);
            scene.set_reciprocal_avoidance(reciprocal);
            // The agent walks a straight line unless avoidance turns it, and its home
            // range does not pull it away from the obstacle.
            scene.tuning().wander.jitter = 0;
            scene.tuning().wander.range = 1e6f;
            const Camera2D camera = session_camera();
            scene.stream(camera);
            const Body obstacle = nearest_terrain(scene, shape);
            // Approach a box across its long side, which is the widest face it has.
            const Vec2 approach =
                rotated(obstacle.shape == Shape::box && obstacle.half_extents.x > obstacle.half_extents.y
                            ? Vec2{0, -1}
                            : Vec2{-1, 0},
                        obstacle.angle);
            const auto id = scene.agents().front().body_id();
            const float start_distance = bounding_radius(obstacle) + start_gap;
            scene.moving_body(id).position = obstacle.position - approach * start_distance;
            scene.moving_body(id).velocity = approach * scene.tuning().motion.max_speed;
            REQUIRE_MESSAGE(!scene.terrain().blocked(scene.world().body(id)),
                            "The agent starts inside terrain");
            float covered = 0;
            for (int tick = 0; tick < ticks; ++tick) {
                INFO("tick " << tick);
                const Vec2 before = scene.world().body(id).position;
                scene.stream(camera);
                scene.update(float(frame_seconds), {});
                const Body &agent = scene.world().body(id);
                REQUIRE_MESSAGE(scene.agents().front().steers_without_route(),
                                "The agent stopped wandering by steering");
                covered += distance(agent.position, before);
                // Without avoidance of terrain the agent runs into the obstacle and physics
                // has to push it out again. Avoidance keeps its clearance, so the shapes
                // never even touch, which also rules out any deeper overlap.
                const auto terrain = scene.world().bodies().subspan(scene.moving_body_count());
                const bool touching = std::any_of(terrain.begin(), terrain.end(), [&](const Body &body) {
                    return collide(agent, body).has_value();
                });
                REQUIRE_MESSAGE(!touching, "A steering wanderer touched terrain");
            }
            const Vec2 end = scene.world().body(id).position;
            INFO("covered " << covered << ", ended " << distance(end, obstacle.position)
                            << " from the obstacle, started " << start_distance);
            // Stopped in front of the obstacle it would have covered the start gap only.
            CHECK_MESSAGE(covered > scene.tuning().motion.max_speed * float(ticks * frame_seconds) / 3,
                          "A steering wanderer did not keep moving at terrain");
            CHECK_MESSAGE(distance(end, obstacle.position) > start_distance,
                          "A steering wanderer stayed at the obstacle it headed for");
        }
}

TEST_CASE("the default tuning equals the former literals of the scene and is valid") {
    const Tuning tuning;
    CHECK(tuning.motion.max_speed == 4);
    CHECK(tuning.motion.max_acceleration == 12);
    CHECK(tuning.motion.response_time == 0.25f);
    CHECK(tuning.motion.lookahead == 0.8f);
    CHECK(tuning.motion.clearance == 0.05f);
    CHECK(tuning.motion.separation_radius == 1.6f);
    CHECK(tuning.motion.separation_weight == 0.65f);
    CHECK(tuning.motion.horizon == 1.5f);
    CHECK(tuning.motion.passing_bias == 0.05f);
    CHECK(slow_radius == 1);
    CHECK(stop_radius == 0.08f);
    CHECK(tuning.wander.mode == WanderMode::steering);
    CHECK(tuning.wander.range == 12);
    CHECK(tuning.wander.reach == 3);
    CHECK(tuning.wander.jitter == 4);
    CHECK_NOTHROW(validate(tuning));
    // The engine's own defaults are where the scene's literals for acceleration, response
    // time, horizon and passing bias came from.
    CHECK(tuning.motion.max_acceleration == Steering{}.max_acceleration);
    CHECK(tuning.motion.response_time == Steering{}.response_time);
    CHECK(tuning.motion.horizon == AvoidanceSettings{}.horizon);
    CHECK(tuning.motion.passing_bias == AvoidanceSettings{}.passing_bias);
    const Application application(0, 42);
    CHECK_MESSAGE((application.tuning().motion.max_speed == 4 &&
                   application.tuning().wander.mode == WanderMode::steering),
                  "A new application does not start from the default tuning");
}

TEST_CASE("tuning validation rejects values that are not finite or out of range") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const auto rejected = [](const Tuning &tuning) {
        try {
            validate(tuning);
        } catch (const std::invalid_argument &) {
            return true;
        }
        return false;
    };
    // Values that must be positive.
    for (const auto member : {&MotionTuning::max_speed, &MotionTuning::max_acceleration,
                              &MotionTuning::response_time, &MotionTuning::horizon})
        for (const float bad : {0.f, -1.f, nan, infinity}) {
            Tuning tuning;
            tuning.motion.*member = bad;
            CHECK_MESSAGE(rejected(tuning), "A motion value that must be positive was accepted");
        }
    for (const auto member : {&WanderTuning::range, &WanderTuning::reach})
        for (const float bad : {0.f, -1.f, nan, infinity}) {
            Tuning tuning;
            tuning.wander.*member = bad;
            CHECK_MESSAGE(rejected(tuning), "A wander value that must be positive was accepted");
        }
    // Values that may be zero, which switches their effect off.
    for (const auto member :
         {&MotionTuning::lookahead, &MotionTuning::clearance, &MotionTuning::separation_radius,
          &MotionTuning::separation_weight, &MotionTuning::passing_bias}) {
        for (const float bad : {-1.f, nan, infinity}) {
            Tuning tuning;
            tuning.motion.*member = bad;
            CHECK_MESSAGE(rejected(tuning), "A negative or non-finite motion value was accepted");
        }
        Tuning tuning;
        tuning.motion.*member = 0;
        CHECK_MESSAGE(!rejected(tuning), "A motion value that may be zero was rejected");
    }
    for (const float bad : {-1.f, nan, infinity}) {
        Tuning tuning;
        tuning.wander.jitter = bad;
        CHECK_MESSAGE(rejected(tuning), "A negative or non-finite wander jitter was accepted");
    }
    Tuning steady;
    steady.wander.jitter = 0;
    CHECK_MESSAGE(!rejected(steady), "A wander without jitter was rejected");
    Tuning unknown_mode;
    unknown_mode.wander.mode = static_cast<WanderMode>(7);
    CHECK_MESSAGE(rejected(unknown_mode), "An unknown wander mode was accepted");

    // The scene refuses to run a tick on invalid values and leaves its bodies alone.
    Application application(4, 42);
    Camera2D camera = session_camera();
    application.advance(frame_seconds, {}, camera);
    const Vec2 before = player_position(application);
    application.tuning().motion.max_speed = nan;
    FrameCommands commands;
    commands.player.direction = {1, 0};
    CHECK_THROWS_AS(application.advance(frame_seconds, commands, camera), std::invalid_argument);
    CHECK_MESSAGE(length(player_position(application) - before) == 0, "A rejected tick moved the player");
    application.tuning() = Tuning{};
    CHECK_NOTHROW(application.advance(frame_seconds, commands, camera));
}

TEST_CASE("a larger top speed makes agents cover more distance in the same time") {
    // Three simulated seconds.
    constexpr int frames = 360;
    Application slow(12, 42), fast(12, 42);
    fast.tuning().motion.max_speed = 8;
    Camera2D slow_camera = session_camera(), fast_camera = session_camera();
    // The first frame streams the population in; measure from the second one.
    slow.advance(frame_seconds, {}, slow_camera);
    fast.advance(frame_seconds, {}, fast_camera);
    const float slow_distance = distance_covered(slow, slow_camera, frames);
    const float fast_distance = distance_covered(fast, fast_camera, frames);
    INFO("covered at 4 units per second: " << slow_distance << ", at 8: " << fast_distance);
    CHECK(slow_distance > 0);
    // Well short of double: with the same acceleration a faster agent takes longer to
    // reach its speed and to turn.
    CHECK(fast_distance > slow_distance * 1.25f);
}

TEST_CASE("half the time scale runs half as many fixed ticks") {
    // Sixteen ticks of elapsed time are the most one frame may run.
    constexpr double elapsed = 16.0 / 120;
    FrameCommands commands;
    commands.player.direction = {1, 0};
    Application half_speed(0, 42), half_time(0, 42), full(0, 42);
    Camera2D camera;
    const Vec2 start = player_position(full);
    full.advance(elapsed, commands, camera);
    half_time.advance(elapsed / 2, commands, camera);
    commands.time_scale = 0.5f;
    half_speed.advance(elapsed, commands, camera);
    CHECK_MESSAGE(player_position(half_time).x > start.x, "The player did not move");
    CHECK_MESSAGE(length(player_position(half_speed) - player_position(half_time)) == 0,
                  "Half the time scale did not run the ticks of half the time");
    CHECK_MESSAGE(player_position(full).x > player_position(half_speed).x,
                  "Half the time scale ran as far as full speed");
    // Scaled time accumulates like real time: two half ticks make one.
    Application slow(0, 42), reference(0, 42);
    slow.advance(1.0 / 120, commands, camera);
    CHECK_MESSAGE(length(player_position(slow) - start) == 0, "Half a tick of scaled time ran a tick");
    slow.advance(1.0 / 120, commands, camera);
    commands.time_scale = 1;
    reference.advance(1.0 / 120, commands, camera);
    CHECK_MESSAGE(length(player_position(slow) - player_position(reference)) == 0,
                  "Two half ticks of scaled time did not run exactly one tick");
    commands.time_scale = 0;
    reference.advance(1, commands, camera);
    CHECK_MESSAGE(length(player_position(slow) - player_position(reference)) == 0,
                  "A time scale of zero advanced the scene");
}

TEST_CASE("a single step while paused runs exactly one fixed tick") {
    Application stepped(6, 42), reference(6, 42);
    Camera2D camera = session_camera(), reference_camera = session_camera();
    FrameCommands commands;
    commands.player.direction = {1, 0};
    reference.advance(1.0 / 120, commands, reference_camera);
    const auto positions = [](const Application &application) {
        std::vector<Vec2> result;
        for (const auto &body : application.scene().world().bodies())
            result.push_back(body.position);
        return result;
    };
    const auto same = [](const std::vector<Vec2> &a, const std::vector<Vec2> &b) {
        return a.size() == b.size() &&
               std::equal(a.begin(), a.end(), b.begin(), [](Vec2 p, Vec2 q) { return length(p - q) == 0; });
    };
    commands.toggle_pause = true;
    stepped.advance(0.1, commands, camera);
    const auto paused = positions(stepped);
    REQUIRE_MESSAGE((stepped.paused() && !same(paused, positions(reference))), "Pausing ran a tick");
    commands.toggle_pause = false;
    commands.single_step = true;
    // A tenth of a second is worth twelve ticks, but a step is one.
    stepped.advance(0.1, commands, camera);
    CHECK_MESSAGE(stepped.paused(), "A single step ended the pause");
    CHECK_MESSAGE(same(positions(stepped), positions(reference)),
                  "A single step did not move every body by exactly one tick");
    const auto player = stepped.scene().player();
    CHECK_MESSAGE(length(stepped.presented_pose(player).position - player_position(stepped)) == 0,
                  "A stepped scene is not drawn at its exact state");
    commands.single_step = false;
    stepped.advance(0.1, commands, camera);
    CHECK_MESSAGE(same(positions(stepped), positions(reference)), "The scene ran on after a single step");
    // While the scene runs, a single step adds nothing to the frame's own ticks.
    commands.toggle_pause = true;
    stepped.advance(0, commands, camera);
    commands.toggle_pause = false;
    commands.single_step = true;
    stepped.advance(1.0 / 120, commands, camera);
    commands.single_step = false;
    reference.advance(1.0 / 120, commands, reference_camera);
    CHECK_MESSAGE(same(positions(stepped), positions(reference)),
                  "A single step changed the ticks of a running scene");
}

TEST_CASE("a negative or non-finite time scale is rejected before the frame changes anything") {
    Application application(0, 42);
    Camera2D camera;
    application.advance(1.0 / 120, {}, camera);
    const Vec2 before = player_position(application);
    for (const float bad :
         {-0.5f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        FrameCommands commands;
        commands.time_scale = bad;
        commands.toggle_pause = commands.toggle_help = true;
        commands.player.direction = {1, 0};
        CHECK_THROWS_AS(application.advance(1.0 / 120, commands, camera), std::invalid_argument);
        CHECK_MESSAGE((!application.paused() && !application.presentation().help &&
                       length(player_position(application) - before) == 0),
                      "A rejected frame changed the application");
    }
}

TEST_CASE("switching avoidance mode neither resets nor advances the scene") {
    Application application(10, 42);
    Camera2D camera;
    FrameCommands commands;
    application.advance(1.0 / 120, commands, camera);
    CHECK_MESSAGE(application.scene().reciprocal_avoidance(), "Reciprocal avoidance did not start enabled");
    const auto count = application.scene().agents().size();
    const auto before = player_position(application);
    commands.reciprocal_avoidance = false;
    application.advance(0, commands, camera);
    CHECK_MESSAGE((!application.scene().reciprocal_avoidance() &&
                   application.scene().agents().size() == count &&
                   length(before - player_position(application)) == 0),
                  "Switching avoidance mode reset or advanced the scene");
    commands.reciprocal_avoidance = true;
    application.advance(0, commands, camera);
    CHECK_MESSAGE(application.scene().reciprocal_avoidance(), "Could not restore reciprocal avoidance");
}

TEST_CASE("help toggles and pause stops fixed updates without replaying paused time") {
    Application application(0, 42);
    Camera2D camera;
    FrameCommands commands;
    CHECK_MESSAGE(!application.presentation().help, "Demo help must start hidden");
    commands.toggle_help = true;
    application.advance(0, commands, camera);
    CHECK_MESSAGE(application.presentation().help, "H did not show Demo help");
    application.advance(0, commands, camera);
    CHECK_MESSAGE(!application.presentation().help, "H did not hide Demo help");
    commands = {};
    commands.toggle_pause = true;
    commands.player.direction = {1, 0};
    const Vec2 start = player_position(application);
    application.advance(0.1, commands, camera);
    CHECK_MESSAGE((application.paused() && length(player_position(application) - start) == 0),
                  "Pausing must precede fixed updates");
    commands.toggle_pause = false;
    application.advance(0.1, commands, camera);
    commands.toggle_pause = true;
    application.advance(0, commands, camera);
    CHECK_MESSAGE((!application.paused() && length(player_position(application) - start) == 0),
                  "Resuming replayed paused time");
    commands.toggle_pause = false;
    application.advance(1.0 / 120, commands, camera);
    CHECK_MESSAGE(player_position(application).x > start.x, "Player movement did not resume");
}

TEST_CASE("ghost input pans the camera and the zoom limit depends on debug mode") {
    Application application(0, 42);
    Camera2D camera;
    FrameCommands commands;
    application.advance(0, commands, camera);
    CHECK_MESSAGE(length(camera.position - player_position(application)) == 0,
                  "Default camera must follow player");
    commands.debug = commands.ghost = true;
    commands.camera_pan = {1, 0};
    commands.player.direction = {1, 0};
    const auto start = player_position(application);
    application.advance(0.1, commands, camera);
    CHECK_MESSAGE((camera.position.x > start.x && length(player_position(application) - start) == 0),
                  "Ghost input moved player or failed to pan camera");
    commands = {};
    commands.zoom_steps = -100;
    application.advance(0, commands, camera);
    CHECK_MESSAGE(camera.pixels_per_unit == 17, "Normal zoom must be limited");
    commands.debug = true;
    application.advance(0, commands, camera);
    CHECK_MESSAGE(camera.pixels_per_unit == 5.f, "Debug zoom must extend visibility");
    commands = {};
    application.advance(0, commands, camera);
    CHECK_MESSAGE(length(camera.position - player_position(application)) == 0,
                  "Leaving ghost failed to follow");
}

TEST_CASE("pointer commands report walking teleport route and agent results") {
    Application application(6, 42);
    Camera2D camera;
    application.advance(0, {}, camera);
    FrameCommands commands;
    commands.toggle_pause = true;
    commands.pointer = PointerCommand{PointerAction::walk, camera.world_to_screen({12, 0})};
    application.advance(0, commands, camera);
    CHECK_MESSAGE((application.last_action() == ActionResult::walking && application.scene().player_goal()),
                  "Walk command lost its route");
    const auto original_path = application.scene().player_path();
    std::vector<Vec2> original_homes;
    for (const auto &agent : application.scene().agents())
        original_homes.push_back(agent.home());
    const Vec2 original_position = player_position(application);
    commands = {};
    commands.debug = commands.ghost = true;
    commands.pointer = PointerCommand{PointerAction::teleport, camera.world_to_screen({9999, 9999})};
    application.advance(0, commands, camera);
    CHECK_MESSAGE((application.last_action() == ActionResult::teleport_blocked &&
                   length(player_position(application) - original_position) == 0 &&
                   application.scene().player_path().size() == original_path.size() &&
                   application.scene().player_goal()),
                  "Rejected teleport changed the player or route");
    commands.debug = commands.ghost = false;
    commands.pointer->action = PointerAction::walk;
    application.advance(0, commands, camera);
    CHECK_MESSAGE((application.last_action() == ActionResult::no_route &&
                   !application.scene().player_goal() && application.scene().player_path().empty()),
                  "Failed walk retained the previous route");
    commands.pointer = PointerCommand{PointerAction::target_agents, camera.world_to_screen({3, 4})};
    application.advance(0, commands, camera);
    CHECK_MESSAGE(application.last_action() == ActionResult::agent_goals_set, "Agent command result lost");
    CHECK_MESSAGE(!application.scene().agents().empty(), "Missing agents for command test");
    for (const auto &agent : application.scene().agents())
        CHECK_MESSAGE((!agent.active() || length(agent.goal() - Vec2{3, 4}) == 0),
                      "Active agent missed command");

    commands.debug = commands.ghost = true;
    commands.pointer = PointerCommand{PointerAction::walk, camera.world_to_screen({0, 0})};
    application.advance(0, commands, camera);
    CHECK_MESSAGE(
        (application.last_action() == ActionResult::teleported && length(player_position(application)) == 0),
        "Teleport before reset failed");
}

TEST_CASE("clicking the same agent keeps it selected and closing clears the selection in Ghost") {
    Application application(6, 42);
    Camera2D camera;
    application.advance(0, {}, camera);
    const auto &scene = application.scene();
    const auto agent = std::find_if(scene.agents().begin(), scene.agents().end(),
                                    [](const auto &candidate) { return candidate.active(); });
    REQUIRE_MESSAGE(agent != scene.agents().end(), "Missing active agent for selection test");
    Input input;
    input.clicked = true;
    input.mouse = camera.world_to_screen(scene.world().body(agent->body_id()).position);
    application.advance(0, read_commands(input), camera);
    CHECK_MESSAGE(scene.selected_agent().has_value(), "Left click did not select an agent");
    application.advance(0, read_commands(input), camera);
    CHECK(scene.selected_agent().has_value());

    const auto start = player_position(application);
    input.clicked = false;
    input.right_clicked = true;
    input.mouse = camera.world_to_screen({12, 0});
    auto commands = read_commands(input);
    commands.debug = commands.ghost = true;
    application.advance(0, commands, camera);
    CHECK(scene.selected_agent().has_value());
    commands.pointer = PointerCommand{PointerAction::clear_selection, input.mouse};
    application.advance(0, commands, camera);
    CHECK_MESSAGE((!scene.selected_agent() && length(player_position(application) - start) == 0 &&
                   application.last_action() == ActionResult::none),
                  "Closing the inspector must clear the selection without teleporting the player");
}

TEST_CASE("opposing held rotation cancels navigation without rotating") {
    Scene scene(0, 42);
    scene.stream(Camera2D{});
    CHECK_MESSAGE(scene.walk_to({12, 0}), "Missing initial route");
    PlayerControl control;
    control.turn_left = control.turn_right = true;
    scene.update(1.f / 120, control);
    CHECK_MESSAGE((!scene.player_goal() && scene.player_path().empty() &&
                   scene.world().body(scene.player()).angular_velocity == 0),
                  "Opposing held rotation controls must cancel navigation without rotating");
}

TEST_CASE("the command adapter keeps held controls and maps clicks to walking") {
    Input input;
    input.held[static_cast<std::size_t>(Key::w)] = true;
    input.held[static_cast<std::size_t>(Key::q)] = true;
    input.held[static_cast<std::size_t>(Key::e)] = true;
    input.held[static_cast<std::size_t>(Key::right)] = true;
    input.held[static_cast<std::size_t>(Key::space)] = true;
    input.pressed[static_cast<std::size_t>(Key::h)] = true;
    input.wheel = -2;
    auto commands = demo::read_commands(input);
    CHECK_MESSAGE((commands.player.direction.y == 1 && commands.player.turn_left &&
                   commands.player.turn_right && commands.camera_pan.x == 1 && commands.zoom_steps == -2),
                  "Held controls lost in command adapter");
    CHECK_MESSAGE((!commands.toggle_pause && !commands.toggle_help && !commands.pointer),
                  "Held keys became toggles or an unclicked pointer became a command");
    input.held[static_cast<std::size_t>(Key::shift)] = true;
    commands = demo::read_commands(input);
    CHECK_MESSAGE((commands.player.turn_right && commands.player.direction.x == 0),
                  "Shift+arrow rotation also translated the player");
    input.clicked = true;
    input.mouse = {12, 34};
    commands = demo::read_commands(input);
    CHECK_MESSAGE((commands.pointer->action == demo::PointerAction::walk &&
                   commands.pointer->screen_position.x == 12 && commands.pointer->screen_position.y == 34),
                  "Plain click was not mapped to walking at logical mouse coordinates");
    input.held[static_cast<std::size_t>(Key::c)] = true;
    CHECK_MESSAGE(demo::read_commands(input).pointer->action == demo::PointerAction::walk,
                  "Agent click mapping changed");
    input.held[static_cast<std::size_t>(Key::t)] = true;
    CHECK_MESSAGE(demo::read_commands(input).pointer->action == demo::PointerAction::walk,
                  "T must not change click behavior");
}

TEST_CASE("drawn poses lie between the last two ticks and snap after a teleport or pause") {
    Application application(0, 42);
    Camera2D camera;
    FrameCommands commands;
    commands.player.direction = {1, 0};
    // Ten whole ticks and half of the next one.
    application.advance(10.5 / 120, commands, camera);
    const auto player = application.scene().player();
    const auto exact = player_position(application);
    const auto drawn = application.presented_pose(player).position;
    CHECK_MESSAGE(length(exact - drawn) > 0, "A moving player was drawn at its simulated position");
    // The player moves at most 4 units per second, so half a tick is under 4 / 240 units.
    CHECK_MESSAGE(length(exact - drawn) < 4.f / 240, "The drawn player is not within the last tick");
    commands = {};
    commands.debug = commands.ghost = true;
    commands.pointer = PointerCommand{PointerAction::walk, camera.world_to_screen({0, 0})};
    application.advance(0, commands, camera);
    REQUIRE(application.last_action() == ActionResult::teleported);
    CHECK_MESSAGE(length(application.presented_pose(player).position - player_position(application)) == 0,
                  "A teleported player was drawn between its old and new place");
    commands = {};
    commands.player.direction = {1, 0};
    application.advance(10.5 / 120, commands, camera);
    commands.toggle_pause = true;
    application.advance(0, commands, camera);
    CHECK_MESSAGE(length(application.presented_pose(player).position - player_position(application)) == 0,
                  "A paused scene must show its exact state");
}
