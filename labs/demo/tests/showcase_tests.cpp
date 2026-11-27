// Demo showcases: agents that hold on arrival and the scripts that send them from stop to
// stop, in a hand-built arena.
//
// The cases assert outcomes (who turns when, who arrives where, no deep overlaps), not
// recorded lines, so they hold on every toolchain.
#include "demo/application.h"

#include <kinetra/simulation/physics.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using demo::Scenario;
using demo::Scene;
using demo::Showcase;
using demo::Turn;
using kinetra::Body;
using kinetra::Vec2;

/// The Demo's fixed tick in seconds.
constexpr float tick = 1.f / 120;
/// Distance within which an agent counts as at a stop: the agent's own arrival threshold
/// and the little that steering and neighbors move a holding agent.
constexpr float at_stop = 0.7f;

Body wall(Vec2 position, Vec2 half_extents) {
    Body body;
    body.shape = kinetra::Shape::box;
    body.motion = kinetra::Motion::fixed;
    body.position = position;
    body.half_extents = half_extents;
    return body;
}

Body mover(Vec2 position, float radius = 0.45f) {
    Body body;
    body.motion = kinetra::Motion::controlled;
    body.position = position;
    body.radius = radius;
    body.friction = 0;
    return body;
}

/// An empty arena of the authored size with the player in a corner and one agent at each
/// of `starts`; the agents hold no showcase yet.
Scene arena(const std::vector<Vec2> &starts) {
    Scene scene(0, 7, Scenario::gravity);
    std::vector movers{mover({-20, -12})};
    std::vector<std::size_t> agents;
    for (const auto start : starts) {
        agents.push_back(movers.size());
        movers.push_back(mover(start));
    }
    scene.arrange(std::move(movers), 0,
                  {wall({0, -14.5f}, {23, 0.5f}), wall({0, 14.5f}, {23, 0.5f}), wall({-22.5f, 0}, {0.5f, 14}),
                   wall({22.5f, 0}, {0.5f, 14})},
                  {{-22, -14}, {22, 14}});
    scene.place_agents(agents, 7);
    return scene;
}

Vec2 position(const Scene &scene, std::size_t agent) {
    return scene.world().body(scene.agents()[agent].body_id()).position;
}

} // namespace

TEST_CASE("an agent that holds on arrival stays at its goal until it is commanded again") {
    auto scene = arena({{-10, 0}});
    scene.set_showcase(Showcase::shuttle({{{-10, 0}, {10, 0}}}, Turn::alone, 1000));
    const auto &agent = scene.agents().front();
    CHECK_FALSE(agent.holding());
    CHECK(agent.goal() == Vec2{10, 0});
    for (int t = 0; t < 120 * 8 && !agent.holding(); ++t)
        scene.update(tick, {});
    REQUIRE(agent.holding());
    CHECK(agent.wandering());
    CHECK(agent.route().empty());
    // The pause is long: the agent stays where it arrived instead of wandering home.
    for (int t = 0; t < 120 * 5; ++t)
        scene.update(tick, {});
    CHECK(agent.holding());
    CHECK(kinetra::length(position(scene, 0) - Vec2{10, 0}) < at_stop);

    scene.command_agent(0, {0, 5});
    CHECK_FALSE(agent.holding());
    CHECK_FALSE(agent.wandering());
}

TEST_CASE("a holding agent whose leg runs out of time holds where it stands") {
    // A wall closes the arena's right half, so the goal cannot be reached.
    auto scene = arena({{-10, 0}});
    scene.arrange({scene.world().body(0), scene.world().body(1)}, 0,
                  {wall({0, -14.5f}, {23, 0.5f}), wall({0, 14.5f}, {23, 0.5f}), wall({-22.5f, 0}, {0.5f, 14}),
                   wall({22.5f, 0}, {0.5f, 14}), wall({0, 0}, {0.5f, 14})},
                  {{-22, -14}, {22, 14}});
    scene.set_showcase(Showcase::shuttle({{{-10, 0}, {10, 0}}}, Turn::alone, 1000));
    const auto &agent = scene.agents().front();
    for (int t = 0; t < 120 * 21 && !agent.holding(); ++t)
        scene.update(tick, {});
    REQUIRE(agent.holding());
    CHECK(position(scene, 0).x < 0);
    CHECK(kinetra::length(agent.goal() - position(scene, 0)) < at_stop);
}

TEST_CASE("agents that turn together wait for the last one and turn in the same tick") {
    // The first agent's leg is half as long as the second's.
    auto scene = arena({{-5, 4}, {-10, -4}});
    scene.set_showcase(Showcase::shuttle({{{-5, 4}, {5, 4}}, {{-10, -4}, {10, -4}}}, Turn::together, 2));
    const auto agents = scene.agents();
    int first_held = -1, second_held = -1, turned = -1;
    for (int t = 0; t < 120 * 20 && turned < 0; ++t) {
        scene.update(tick, {});
        if (first_held < 0 && agents[0].holding())
            first_held = t;
        if (second_held < 0 && agents[1].holding())
            second_held = t;
        if (second_held >= 0 && !agents[0].holding()) {
            // The first agent left: the second must leave in the same tick.
            CHECK_FALSE(agents[1].holding());
            turned = t;
        }
    }
    REQUIRE(first_held >= 0);
    REQUIRE(second_held > first_held + 120);
    REQUIRE(turned >= 0);
    // Two seconds after the last one arrived, within the tick in which the pause ends.
    CHECK(std::abs(float(turned - second_held) * tick - 2) <= 2 * tick);
    CHECK(agents[0].goal() == Vec2{-5, 4});
    CHECK(agents[1].goal() == Vec2{-10, -4});
}

TEST_CASE("agents that turn alone each turn after their own pause") {
    auto scene = arena({{-5, 4}, {-10, -4}});
    scene.set_showcase(Showcase::shuttle({{{-5, 4}, {5, 4}}, {{-10, -4}, {10, -4}}}, Turn::alone, 1));
    const auto agents = scene.agents();
    std::vector<int> held(2, -1), left(2, -1);
    for (int t = 0; t < 120 * 12 && (left[0] < 0 || left[1] < 0); ++t) {
        scene.update(tick, {});
        for (std::size_t i = 0; i < 2; ++i) {
            if (held[i] < 0 && agents[i].holding())
                held[i] = t;
            if (held[i] >= 0 && left[i] < 0 && !agents[i].holding())
                left[i] = t;
        }
    }
    REQUIRE(left[0] >= 0);
    REQUIRE(left[1] >= 0);
    // The short leg turns back before the long one has even arrived.
    CHECK(left[0] < held[1]);
    for (std::size_t i = 0; i < 2; ++i)
        CHECK(std::abs(float(left[i] - held[i]) * tick - 1) <= 2 * tick);
}

TEST_CASE("roaming agents travel to seeded points inside the area, the same ones for the same seed") {
    const auto goals = [](std::uint32_t seed) {
        auto scene = arena({{-10, 0}, {10, 0}});
        scene.set_showcase(Showcase::roam(2, {{-8, -6}, {8, 6}}, 0.5f, seed));
        std::vector<Vec2> seen;
        for (int t = 0; t < 120 * 20; ++t) {
            scene.update(tick, {});
            for (const auto &agent : scene.agents())
                if (!agent.holding() && (seen.empty() || !(seen.back() == agent.goal())) &&
                    std::ranges::find(seen, agent.goal()) == seen.end())
                    seen.push_back(agent.goal());
        }
        return seen;
    };
    const auto first = goals(5);
    CHECK(first.size() >= 6);
    for (const auto goal : first) {
        CHECK(goal.x >= -8.f);
        CHECK(goal.x <= 8.f);
        CHECK(goal.y >= -6.f);
        CHECK(goal.y <= 6.f);
    }
    CHECK(goals(5) == first);
    CHECK_FALSE(goals(6) == first);
}

TEST_CASE("following agents keep near their slots, and a commanded one is left alone until it holds") {
    const std::vector<Vec2> square{{-8, -6}, {8, -6}, {8, 6}, {-8, 6}};
    auto showcase =
        Showcase::follow({{square, 2, 0}}, {{0, {0, 0}}, {0, {-2, 1.5f}}, {0, {-2, -1.5f}}}, true);
    std::vector<Vec2> starts;
    for (std::size_t i = 0; i < 3; ++i)
        starts.push_back(showcase.slot_target(i));
    auto scene = arena(starts);
    scene.set_showcase(std::move(showcase));
    float error = 0;
    int samples = 0;
    for (int t = 0; t < 120 * 16; ++t) {
        scene.update(tick, {});
        if (t > 120 * 2) {
            for (std::size_t i = 0; i < 3; ++i)
                error += kinetra::length(position(scene, i) - scene.showcase()->slot_target(i));
            ++samples;
        }
    }
    CHECK(error / float(3 * samples) < 2);
    // In 16 seconds the lure has travelled 32 units around the square.
    CHECK(kinetra::length(scene.showcase()->lure_position(0) - Vec2{4, 6}) < 0.05f);

    scene.command_agent(1, {-18, 10});
    const auto &agent = scene.agents()[1];
    for (int t = 0; t < 120 * 2; ++t)
        scene.update(tick, {});
    CHECK(agent.goal() == Vec2{-18, 10});
    for (int t = 0; t < 120 * 10 && !agent.holding(); ++t)
        scene.update(tick, {});
    REQUIRE(agent.holding());
    scene.update(tick, {});
    for (int t = 0; t < 30 && agent.goal() == Vec2{-18, 10}; ++t)
        scene.update(tick, {});
    CHECK_FALSE(agent.goal() == Vec2{-18, 10});
}

TEST_CASE("showcase scripts reject what they cannot run") {
    CHECK_THROWS_AS(Showcase::shuttle({{{0, 0}}}, Turn::alone, 1), std::invalid_argument);
    CHECK_THROWS_AS(Showcase::shuttle({{{0, 0}, {1, 0}}}, Turn::alone, -1), std::invalid_argument);
    CHECK_THROWS_AS(Showcase::roam(1, {{1, 0}, {0, 1}}, 1, 0), std::invalid_argument);
    CHECK_THROWS_AS(Showcase::follow({{{{0, 0}}, 1, 0}}, {}, false), std::invalid_argument);
    CHECK_THROWS_AS(Showcase::follow({{{{0, 0}, {1, 0}}, 1, 0}}, {{1, {}}}, false), std::invalid_argument);
    auto scene = arena({{-10, 0}});
    CHECK_THROWS_AS(scene.set_showcase(Showcase::roam(2, {{-1, -1}, {1, 1}}, 1, 0)), std::invalid_argument);
    CHECK_FALSE(scene.showcase());
    scene.set_showcase(Showcase::roam(1, {{-1, -1}, {1, 1}}, 1, 0));
    CHECK(scene.showcase());
    const std::size_t body = scene.agents().front().body_id();
    scene.place_agents(std::vector{body}, 7);
    CHECK_FALSE(scene.showcase());
}

