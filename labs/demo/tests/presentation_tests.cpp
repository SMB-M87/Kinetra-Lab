// Graphics tests for the Demo inspector, routes and steering probes, and for routes drawn
// under the bodies they cross.
#include "demo/presentation/renderer.h"
#include "demo/presentation/session.h"
#include "demo/zoom.h"

#include <kinetra/runtime/engine.h>

#include <doctest/doctest.h>
#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace kinetra;

TEST_CASE("Demo inspector coexists with group goals, Ghost teleport and debug pause") {
    Runtime runtime("Demo inspector", 1280, 800);
    auto session = demo::create_session(6, 42);
    auto &camera = runtime.camera;
    camera.pixels_per_unit = demo::initial_zoom;
    session->advance(0, {}, camera);
    const auto &application = session->application();
    const auto &scene = application.scene();
    Input menu;
    menu.pressed[std::size_t(Key::shift)] = menu.pressed[std::size_t(Key::control)] = true;
    session->advance(0, menu, camera);
    const auto click = [&](Vec2 point) {
        Input input;
        input.clicked = true;
        input.mouse = point;
        session->advance(0, input, camera);
    };
    const auto row = [&](std::string_view label) {
        const auto rows = session->debug_rows();
        const auto found = std::ranges::find(rows, label, &kinetra_lab::DebugRow::label);
        REQUIRE(found != rows.end());
        click(Vec2{8, 32} + Vec2{20, DebugMenu::row_top(std::size_t(found - rows.begin())) + 8} *
                                DebugMenu::scale(camera.viewport));
    };
    const auto agents = scene.agents();
    const auto active = std::ranges::find_if(agents, [&](const auto &agent) {
        const auto position = camera.world_to_screen(scene.world().body(agent.body_id()).position);
        return agent.active() && position.x > 350 && position.x < 900 && position.y > 300 && position.y < 700;
    });
    REQUIRE(active != agents.end());
    click(camera.world_to_screen(scene.world().body(active->body_id()).position));
    REQUIRE(scene.selected_agent());
    row("AGENTS");
    row("CLICK SETS GOAL");
    click(camera.world_to_screen({0, 0}));
    CHECK(application.last_action() == demo::ActionResult::agent_goals_set);
    CHECK(scene.selected_agent());
    row("GHOST CAMERA");
    click(camera.world_to_screen({0, 0}));
    REQUIRE(application.last_action() == demo::ActionResult::teleported);
    REQUIRE(scene.selected_agent());
    const auto player = scene.world().body(scene.player()).position;
    row("SIMULATION");
    row("RUNNING");
    CHECK(application.paused());
    click({1260, 20});
    CHECK_FALSE(scene.selected_agent());
    CHECK(length(scene.world().body(scene.player()).position - player) == 0);
    CHECK(application.last_action() == demo::ActionResult::teleported);
}

TEST_CASE("routes and steering probes render at different camera zoom levels") {
    Runtime runtime("Demo graphics", 160, 120);
    runtime.read_back(true);
    runtime.camera.pixels_per_unit = 73;
    demo::Application application(0, 42);
    application.advance(0, {}, runtime.camera);
    demo::render(runtime, application);
    demo::FrameCommands debug;
    debug.toggle_paths = debug.toggle_vectors = true;
    debug.pointer = demo::PointerCommand{demo::PointerAction::walk, runtime.camera.world_to_screen({0, -17})};
    application.advance(0, debug, runtime.camera);
    REQUIRE_MESSAGE(application.last_action() == demo::ActionResult::walking,
                    "Missing route for renderer check");
    demo::render(runtime, application);
    CHECK_MESSAGE(glGetError() == GL_NO_ERROR, "Scene renderer reported an OpenGL error");
    demo::FrameCommands probes;
    probes.probes = true;
    probes.toggle_paths = probes.toggle_vectors = true;
    application.advance(0, probes, runtime.camera);
    const auto &player = application.scene().world().body(application.scene().player());
    for (float zoom : {5.f, 73.f}) {
        CAPTURE(zoom);
        runtime.camera.pixels_per_unit = zoom;
        runtime.camera.position = player.position + rotated(Vec2{3, 0}, player.angle);
        demo::render(runtime, application);
        // The pixel just above the window's center, as OpenGL's bottom-up row height / 2 was.
        const auto &frame = runtime.frame();
        const auto pixel = frame.pixel(frame.width / 2, frame.height - 1 - frame.height / 2);
        CHECK_MESSAGE((pixel[0] > 240 && pixel[1] >= 150 && pixel[1] <= 180),
                      "Steering probe disappeared at camera zoom");
    }
}

TEST_CASE("a route passes under the agents it crosses and starts over its own body") {
    Runtime runtime("Demo routes under bodies", 1280, 800);
    runtime.read_back(true);
    runtime.camera.pixels_per_unit = 30;
    // The doorway scenario places its own six agents, in two groups of three.
    demo::Application application(0, 7, demo::Scenario::doorway);
    application.advance(0, {}, runtime.camera);
    const auto &scene = application.scene();
    const auto &world = scene.world();
    demo::FrameCommands paths;
    paths.toggle_paths = true;
    application.advance(0, paths, runtime.camera);
    const auto player_color = [](std::array<std::uint8_t, 4> pixel) {
        // The player's route, {0.6, 1, 0.3}.
        return std::abs(pixel[0] - 153) < 30 && pixel[1] > 225 && std::abs(pixel[2] - 77) < 30;
    };
    const auto screen = [&](Vec2 point) {
        const auto at = runtime.camera.world_to_screen(point);
        return std::pair{int(std::lround(at.x)), int(std::lround(at.y))};
    };
    // Walk the player to a point just beyond each agent in turn until its route crosses the
    // middle of one, away from that agent's own route start and heading line.
    std::optional<Vec2> crossing;
    const auto player = application.presented_pose(scene.player()).position;
    for (const auto &target_agent : scene.agents()) {
        const auto beyond = application.presented_pose(target_agent.body_id()).position;
        // The scenario places the camera; click through the camera it left.
        application.advance(0, {}, runtime.camera);
        demo::FrameCommands walk;
        walk.pointer =
            demo::PointerCommand{demo::PointerAction::walk,
                                 runtime.camera.world_to_screen(beyond + normalized(beyond - player) * 2.f)};
        application.advance(0, walk, runtime.camera);
        if (application.last_action() != demo::ActionResult::walking)
            continue;
        Vec2 from = player;
        for (const auto waypoint : scene.player_path()) {
            for (const auto &agent : scene.agents()) {
                const auto &body = world.body(agent.body_id());
                const auto pose = application.presented_pose(agent.body_id());
                const auto segment = waypoint - from;
                const float t =
                    std::clamp(dot(pose.position - from, segment) / dot(segment, segment), 0.f, 1.f);
                // A point of the route inside the agent but off its center, where the agent's
                // heading line and own route start begin.
                auto point = from + segment * t;
                if (length(point - pose.position) < 0.25f * body.radius)
                    point = point + normalized(segment) * (0.4f * body.radius);
                const auto away = [&](Vec2 direction) {
                    return length(direction) == 0 ||
                           dot(normalized(direction), normalized(point - pose.position)) < 0.7f;
                };
                const bool on_route = distance_to_segment(point, from, waypoint) < 0.01f &&
                                      length(point - pose.position) < 0.65f * body.radius;
                const bool own_route_clear =
                    agent.route().empty() || away(agent.route().front() - pose.position);
                if (on_route && length(pose.position - player) > 3 && own_route_clear &&
                    away(rotated(Vec2{1, 0}, pose.angle))) {
                    crossing = point;
                    break;
                }
            }
            if (crossing)
                break;
            from = waypoint;
        }
        if (crossing)
            break;
    }
    REQUIRE_MESSAGE(crossing, "No route of the player crossed an agent");
    demo::render(runtime, application);
    const auto &frame = runtime.frame();
    const auto [x, y] = screen(*crossing);
    CHECK_MESSAGE(!player_color(frame.pixel(x, y)), "The player's route covers an agent it crosses");
    // The route starts over the player itself: a pixel on its first segment, inside its body.
    const auto first = scene.player_path().front() - player;
    const auto [px, py] = screen(player + normalized(first) * (0.6f * world.body(scene.player()).radius));
    CHECK_MESSAGE(player_color(frame.pixel(px, py)), "The start of the player's route is hidden");
}

TEST_CASE("steering probes pass under the agents they cross") {
    Runtime runtime("Demo probes under bodies", 1280, 800);
    runtime.read_back(true);
    runtime.camera.pixels_per_unit = 30;
    demo::Application application(48, 123);
    const auto &scene = application.scene();
    const auto &world = scene.world();
    for (int frame = 0; frame < 120; ++frame)
        application.advance(1.0 / 60, {}, runtime.camera);
    demo::FrameCommands probes;
    probes.probes = true;
    application.advance(0, probes, runtime.camera);
    const auto probe_color = [](std::array<std::uint8_t, 4> pixel) {
        // The probes, {1, 0.65, 0.2}.
        return pixel[0] > 240 && std::abs(pixel[1] - 166) < 15 && std::abs(pixel[2] - 51) < 20;
    };
    // Points (world space) where a probe of one moving body crosses the middle of an agent, as
    // the renderer draws both this frame.
    std::vector<Vec2> crossings;
    for (std::size_t id = 0; id < world.bodies().size(); ++id) {
        const auto &body = world.body(id);
        if (body.motion == Motion::fixed || !scene.view().overlaps(bounds(body)))
            continue;
        const auto pose = application.presented_pose(id);
        const auto direction =
            length(body.velocity) > 0.01f ? normalized(body.velocity) : rotated(Vec2{1, 0}, pose.angle);
        for (float angle : {0.f, 2.35619449f, -2.35619449f}) {
            const auto end = pose.position + rotated(direction, angle) * (angle == 0 ? 4.f : 2.f);
            for (const auto &agent : scene.agents()) {
                if (agent.body_id() == id)
                    continue;
                const auto center = application.presented_pose(agent.body_id()).position;
                const auto segment = end - pose.position;
                const auto point = pose.position + segment * std::clamp(dot(center - pose.position, segment) /
                                                                            dot(segment, segment),
                                                                        0.f, 1.f);
                const auto screen = runtime.camera.world_to_screen(point);
                if (length(point - center) < 0.6f * world.body(agent.body_id()).radius && screen.x > 0 &&
                    screen.y > 0 && screen.x < 1280 && screen.y < 800)
                    crossings.push_back(point);
            }
        }
    }
    REQUIRE_MESSAGE(!crossings.empty(), "No probe crossed an agent");
    demo::render(runtime, application);
    const auto &frame = runtime.frame();
    for (const auto point : crossings) {
        CAPTURE(point.x);
        CAPTURE(point.y);
        const auto at = runtime.camera.world_to_screen(point);
        CHECK_MESSAGE(!probe_color(frame.pixel(int(std::lround(at.x)), int(std::lround(at.y)))),
                      "A probe covers an agent it crosses");
    }
}

TEST_CASE("velocity vectors pass under the agents they cross") {
    Runtime runtime("Demo vectors under bodies", 1280, 800);
    runtime.read_back(true);
    runtime.camera.pixels_per_unit = 30;
    demo::Application application(48, 123);
    const auto &scene = application.scene();
    const auto &world = scene.world();
    for (int frame = 0; frame < 120; ++frame)
        application.advance(1.0 / 60, {}, runtime.camera);
    demo::FrameCommands vectors;
    vectors.toggle_vectors = true;
    application.advance(0, vectors, runtime.camera);
    REQUIRE(application.presentation().vectors);
    const auto vector_color = [](std::array<std::uint8_t, 4> pixel) {
        // The vectors, {0.8, 0.95, 0.4}.
        return std::abs(pixel[0] - 204) < 20 && std::abs(pixel[1] - 242) < 15 &&
               std::abs(pixel[2] - 102) < 20;
    };
    // The shaft of a body's vector as draw_debug_vector draws it: 0.3 s of motion, at least
    // 1.5 world units long.
    const auto shaft = [&](std::size_t id) {
        const auto velocity = world.body(id).velocity * 0.3f;
        const auto start = application.presented_pose(id).position;
        const float size = std::max({length(velocity), 1.5f, 24.f / runtime.camera.pixels_per_unit});
        return std::pair{start, length(velocity) > 0 ? start + normalized(velocity) * size : start};
    };
    // Points (world space) where the vector of one moving body crosses the middle of an agent,
    // clear of that agent's own vector, which starts over it.
    std::vector<Vec2> crossings;
    for (std::size_t id = 0; id < world.bodies().size(); ++id) {
        const auto &body = world.body(id);
        if (body.motion == Motion::fixed || !scene.view().overlaps(bounds(body)) ||
            length(body.velocity) == 0)
            continue;
        const auto [start, end] = shaft(id);
        for (const auto &agent : scene.agents()) {
            if (agent.body_id() == id)
                continue;
            const auto center = application.presented_pose(agent.body_id()).position;
            const float radius = world.body(agent.body_id()).radius;
            const auto segment = end - start;
            const auto point =
                start + segment * std::clamp(dot(center - start, segment) / dot(segment, segment), 0.f, 1.f);
            const auto [own_start, own_end] = shaft(agent.body_id());
            const auto screen = runtime.camera.world_to_screen(point);
            if (length(point - center) < 0.6f * radius &&
                distance_to_segment(point, own_start, own_end) > 0.25f * radius && screen.x > 0 &&
                screen.y > 0 && screen.x < 1280 && screen.y < 800)
                crossings.push_back(point);
        }
    }
    REQUIRE_MESSAGE(!crossings.empty(), "No vector crossed an agent");
    demo::render(runtime, application);
    const auto &frame = runtime.frame();
    for (const auto point : crossings) {
        CAPTURE(point.x);
        CAPTURE(point.y);
        const auto at = runtime.camera.world_to_screen(point);
        CHECK_MESSAGE(!vector_color(frame.pixel(int(std::lround(at.x)), int(std::lround(at.y)))),
                      "A vector covers an agent it crosses");
    }
}
