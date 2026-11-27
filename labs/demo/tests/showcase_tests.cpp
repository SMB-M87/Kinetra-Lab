// Demo showcases: agents that hold on arrival and the scripts that send them from stop to
// stop, in a hand-built arena, and every showcase scenario run for a while.
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
using demo::Agent;
using demo::Scenario;
using demo::Scene;
using demo::Showcase;
using demo::Turn;
using kinetra::Body;
using kinetra::Vec2;

/// The Demo's fixed tick in seconds.
constexpr float tick = 1.f / 120;
/// Largest allowed overlap between two moving bodies, in world units: contact correction
/// leaves about 0.001, more means the bodies ran into each other.
constexpr float max_penetration = 0.01f;
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

/// Largest overlap between two moving bodies of the scene right now.
float deepest_overlap(const Scene &scene) {
    float deepest = 0;
    for (std::size_t a = 0; a < scene.moving_body_count(); ++a)
        for (std::size_t b = a + 1; b < scene.moving_body_count(); ++b)
            if (const auto hit = kinetra::collide(scene.world().body(a), scene.world().body(b)))
                deepest = std::max(deepest, hit->penetration);
    return deepest;
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

TEST_CASE("a showcase command ends the passing plan agreed for the old goal") {
    // Two lures run back and forth along one line, half a lap apart. Each agent starts at
    // one end and its slot lies ten units beyond its lure, past the other agent, so the two
    // meet head-on on commanded travel and agree on lanes; every quarter second both get a
    // new goal.
    const std::vector<Vec2> line{{-12, 0}, {12, 0}};
    auto showcase = Showcase::follow({{line, 2, 8}, {line, 2, 32}}, {{0, {10, 0}}, {1, {-10, 0}}}, false);
    auto scene = arena({{-15, 0}, {15, 0}});
    scene.set_passing(true);
    scene.set_showcase(std::move(showcase));
    std::vector<Vec2> goals{scene.agents()[0].goal(), scene.agents()[1].goal()};
    int formed = 0, stale = 0;
    for (int t = 0; t < 120 * 12; ++t) {
        scene.update(tick, {});
        const bool commanded =
            !(scene.agents()[0].goal() == goals[0]) || !(scene.agents()[1].goal() == goals[1]);
        goals = {scene.agents()[0].goal(), scene.agents()[1].goal()};
        const auto events = scene.passing_events();
        const bool formed_now = std::ranges::any_of(events, [](const kinetra::PassingEvent &event) {
            return event.kind == kinetra::PassingEventKind::formed;
        });
        formed += formed_now;
        // A plan that survives a new goal must have been agreed after it, in this tick.
        stale += commanded && !scene.passing_plans().empty() && !formed_now;
    }
    CHECK(formed > 0);
    CHECK(stale == 0);
}

TEST_CASE("a scene in Terrain refuses a showcase, and a dormant agent waits for its leg") {
    Scene terrain(1, 7);
    CHECK_THROWS_AS(terrain.set_showcase(Showcase::roam(1, {{-1, -1}, {1, 1}}, 1, 0)), std::invalid_argument);

    auto showcase = Showcase::shuttle({{{-10, 0}, {10, 0}}}, Turn::together, 0);
    std::vector<Agent> agents{Agent(0, {-10, 0})};
    // The agent never saw the view, so it is dormant.
    agents.front().hold_on_arrival();
    CHECK(showcase.update(tick, agents).empty());
    CHECK(showcase.stop(0) == Vec2{-10, 0});
    Body body = mover({-10, 0});
    agents.front().update_visibility({{-20, -20}, {20, 20}}, body);
    REQUIRE(agents.front().active());
    const auto commanded = showcase.update(tick, agents);
    REQUIRE(commanded.size() == 1);
    CHECK(showcase.stop(0) == Vec2{10, 0});
    CHECK(agents.front().goal() == Vec2{10, 0});
    CHECK_THROWS_AS(showcase.update(-tick, agents), std::invalid_argument);
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

TEST_CASE("every showcase scenario repeats without contact") {
    struct Expectation {
        Scenario scenario;
        std::size_t agents;
        /// Legs (holds after the start) every agent completes within the run.
        int legs;
    };
    // Shuttles that turn together complete a leg in 6 to 14 seconds, Maze's agents in up
    // to 18, roaming agents in about 5; following agents hold whenever they catch up.
    for (const auto [scenario, agents, legs] :
         {Expectation{Scenario::maze, 6, 2}, Expectation{Scenario::doorway, 6, 3},
          Expectation{Scenario::corridor, 12, 2}, Expectation{Scenario::crossing, 12, 2},
          Expectation{Scenario::circle, 16, 3}, Expectation{Scenario::bottleneck, 16, 2},
          Expectation{Scenario::chaos, 40, 3}, Expectation{Scenario::fleet, 12, 2},
          Expectation{Scenario::swarm, 30, 2}}) {
        CAPTURE(demo::scenario_label(scenario));
        Scene scene(0, 42, scenario);
        REQUIRE(scene.agents().size() == agents);
        REQUIRE(scene.showcase());
        std::vector<int> holds(agents);
        std::vector<bool> holding(agents);
        float deepest = 0, farthest_from_stop = 0;
        bool active = true;
        for (int t = 0; t < 120 * 40; ++t) {
            scene.update(tick, {});
            for (std::size_t i = 0; i < agents; ++i) {
                const auto &agent = scene.agents()[i];
                active = active && agent.active();
                if (agent.holding() && !holding[i]) {
                    ++holds[i];
                    // A shuttle's leg ends at its stop, not where its time ran out.
                    if (const auto stop = scene.showcase()->stop(i))
                        farthest_from_stop =
                            std::max(farthest_from_stop, kinetra::length(position(scene, i) - *stop));
                }
                holding[i] = agent.holding();
            }
            deepest = std::max(deepest, deepest_overlap(scene));
        }
        CHECK(active);
        CHECK(*std::ranges::min_element(holds) >= legs);
        CHECK(farthest_from_stop < at_stop);
        CHECK(deepest <= max_penetration);
    }
}
