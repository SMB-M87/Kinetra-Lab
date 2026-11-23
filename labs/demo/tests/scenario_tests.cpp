#include "demo/application.h"
#include "demo/debug_panel.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

namespace {
constexpr double tick = 1.0 / 120;
} // namespace

TEST_CASE("Gravity drops the authored bodies with Debug Mode off") {
    demo::Application application(48, 42, demo::Scenario::gravity);
    demo::DebugPanel panel(application);
    const auto &scene = application.scene();
    REQUIRE(scene.moving_body_count() == 7);
    CHECK(scene.agents().empty());
    const float start_y = scene.world().body(1).position.y;
    kinetra::Camera2D camera;
    for (int frame = 0; frame < 60; ++frame) {
        demo::FrameCommands commands;
        panel.apply(commands, false, false, 0);
        application.advance(tick, commands, camera);
    }
    CHECK(scene.world().gravity.y < 0);
    CHECK(scene.world().body(1).position.y < start_y - 0.5f);
    CHECK(scene.terrain().chunks().empty());
}

TEST_CASE("Maze keeps its walls and route through resize and Ghost movement") {
    demo::Scene scene(48, 42, demo::Scenario::maze);
    const kinetra::Vec2 goal{17, 8};
    REQUIRE(scene.walk_to(goal));
    CHECK(scene.player_path().size() >= 4);
    REQUIRE(scene.player_goal());
    CHECK(kinetra::length(*scene.player_goal() - goal) < 1);
    const auto bodies = scene.world().bodies().size();
    std::vector<kinetra::Aabb> walls;
    for (auto id = scene.moving_body_count(); id < bodies; ++id)
        walls.push_back(kinetra::bounds(scene.world().body(id)));
    const auto check_walls = [&] {
        REQUIRE(scene.world().bodies().size() == bodies);
        for (std::size_t index = 0; index < walls.size(); ++index) {
            const auto wall = kinetra::bounds(scene.world().body(scene.moving_body_count() + index));
            CHECK(kinetra::length(wall.min - walls[index].min) == 0);
            CHECK(kinetra::length(wall.max - walls[index].max) == 0);
        }
    };

    kinetra::Camera2D camera;
    camera.viewport = {1280, 800};
    scene.stream(camera);
    for (int frame = 0; frame < 30; ++frame)
        scene.update(static_cast<float>(tick), {});
    REQUIRE(scene.player_goal());

    camera.viewport = {480, 280};
    scene.stream(camera);
    check_walls();
    REQUIRE(scene.player_goal());
    CHECK(kinetra::length(*scene.player_goal() - goal) < 1);
    REQUIRE(scene.walk_to(goal));
    CHECK(scene.player_path().size() >= 4);

    scene.set_following(false);
    camera.position = {100, 100};
    scene.stream(camera);
    CHECK_FALSE(scene.teleport(camera.position));
    check_walls();
    CHECK(scene.preview().chunks().empty());
    CHECK(scene.terrain().chunks().empty());

    for (int frame = 0; frame < 2400 && scene.player_goal(); ++frame)
        scene.update(static_cast<float>(tick), {});
    CHECK(kinetra::length(scene.world().body(scene.player()).position - goal) < 1);
}

TEST_CASE("Doorway starts opposing agents and applies both avoidance modes") {
    demo::Application application(0, 42, demo::Scenario::doorway);
    const auto &scene = application.scene();
    REQUIRE(scene.agents().size() == 6);
    for (const auto &agent : scene.agents()) {
        CHECK(agent.active());
        CHECK(agent.goal().x == -agent.home().x);
    }
    const auto id = scene.agents().front().body_id();
    const auto start = scene.world().body(id).position;
    kinetra::Camera2D camera;
    for (int frame = 0; frame < 120; ++frame)
        application.advance(tick, {}, camera);
    CHECK(scene.reciprocal_avoidance());
    CHECK(scene.world().body(id).position.x > start.x + 1);
    const auto crossed = [&] {
        return std::ranges::any_of(scene.agents(), [&](const demo::Agent &agent) {
            return agent.home().x < 0 && scene.world().body(agent.body_id()).position.x > 1;
        });
    };
    for (int frame = 0; frame < 1200 && !crossed(); ++frame)
        application.advance(tick, {}, camera);
    CHECK(crossed());

    demo::FrameCommands commands;
    commands.reciprocal_avoidance = false;
    application.advance(tick, commands, camera);
    CHECK_FALSE(scene.reciprocal_avoidance());
}
