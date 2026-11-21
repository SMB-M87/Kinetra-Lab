// Headless tests for the Demo scene: seeded terrain streaming, obstacles, agent
// population and lifecycle, wandering by steering and by route, navigation, the path
// follower and Ghost isolation.
#include "demo/world/scene.h"

#include <kinetra/simulation/avoidance.h>
#include <kinetra/simulation/steering.h>

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

using namespace kinetra;

namespace {
// Compare all generated shape/material fields rather than object padding bytes.
bool same(const Body &a, const Body &b) {
    if (a.vertices.size() != b.vertices.size())
        return false;
    for (std::size_t i = 0; i < a.vertices.size(); ++i)
        if (length(a.vertices[i] - b.vertices[i]) != 0)
            return false;
    return a.shape == b.shape && a.motion == b.motion && a.position.x == b.position.x &&
           a.position.y == b.position.y && a.half_extents.x == b.half_extents.x &&
           a.half_extents.y == b.half_extents.y && a.radius == b.radius && a.angle == b.angle &&
           a.friction == b.friction && a.restitution == b.restitution && a.velocity.x == b.velocity.x &&
           a.velocity.y == b.velocity.y && a.acceleration.x == b.acceleration.x &&
           a.acceleration.y == b.acceleration.y && a.angular_velocity == b.angular_velocity &&
           a.mass == b.mass;
}
bool same(const demo::Terrain::Chunks &a, const demo::Terrain::Chunks &b) {
    if (a.size() != b.size())
        return false;
    for (const auto &[coordinate, bodies] : a) {
        const auto it = b.find(coordinate);
        if (it == b.end() || bodies.size() != it->second.size())
            return false;
        for (std::size_t i = 0; i < bodies.size(); ++i)
            if (!same(bodies[i], it->second[i]))
                return false;
    }
    return true;
}
/// Checks that `terrain` rejects `view` with std::invalid_argument and keeps its chunks.
void rejects(demo::Terrain &terrain, Aabb view) {
    const auto before = terrain.chunks();
    bool rejected = false;
    try {
        terrain.update(view);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    CHECK_MESSAGE((rejected && same(before, terrain.chunks())),
                  "Invalid view changed terrain or was accepted");
}

/// A fixed 2 by 14 box wall at the origin, rotated by `angle` radians.
Body wall_box(float angle) {
    Body obstacle;
    obstacle.motion = Motion::fixed;
    obstacle.shape = Shape::box;
    obstacle.half_extents = {1, 7};
    obstacle.angle = angle;
    return obstacle;
}

/// The default tuning, but with wander by planned routes; the home range stays 12 world units.
demo::Tuning route_wander() {
    demo::Tuning tuning;
    tuning.wander.mode = demo::WanderMode::route;
    return tuning;
}

/// Route wander that never turns back toward home. The cases that pin the legs of a freely
/// roaming agent use it: they were written before agents had a home range.
demo::Tuning free_route_wander() {
    demo::Tuning tuning = route_wander();
    tuning.wander.range = 1e6f;
    return tuning;
}

/// Fixed step of the Demo's clock in seconds.
constexpr float tick_seconds = 1.f / 120;

/// A view that keeps an agent anywhere in these tests active.
constexpr Aabb everywhere{{-1000, -1000}, {1000, 1000}};

/// Moves `body` one tick toward `target` the way the scene moves an agent that has open
/// space around it.
void follow(Body &body, Vec2 target, const demo::Tuning &tuning) {
    Steering settings;
    settings.max_speed = tuning.motion.max_speed;
    settings.max_acceleration = tuning.motion.max_acceleration;
    settings.response_time = tuning.motion.response_time;
    settings.slow_radius = demo::slow_radius;
    settings.stop_radius = demo::stop_radius;
    body.acceleration = steer(body, target, {}, settings);
    integrate(body, tick_seconds);
}

/// The player's controlled box body without friction, at `position`.
Body follower_player(Vec2 position) {
    Body player;
    player.motion = Motion::controlled;
    player.shape = Shape::box;
    player.half_extents = {0.65f, 0.4f};
    player.position = position;
    player.friction = 0;
    return player;
}
} // namespace

TEST_CASE("the demo population includes triangle pentagon and hexagon agents") {
    demo::Scene shape_scene(10, 42);
    std::set<std::size_t> polygon_sides;
    for (const auto &agent : shape_scene.agents()) {
        const auto &body = shape_scene.world.body(agent.body_id());
        validate(body);
        if (body.shape == Shape::polygon)
            polygon_sides.insert(body.vertices.size());
    }
    CHECK_MESSAGE((polygon_sides == std::set<std::size_t>{3, 5, 6}),
                  "Demo population lacks polygon variants");
}

TEST_CASE("seeded terrain unloads distant chunks reloads unchanged and rejects invalid views") {
    demo::Terrain terrain(42), repeat(42), different(43);
    const Aabb origin{{-40, -30}, {40, 30}};
    CHECK_MESSAGE(terrain.update(origin), "Initial terrain not loaded");
    const auto original = terrain.chunks();
    CHECK_MESSAGE(!terrain.update(origin), "Unchanged view rebuilt terrain");
    repeat.update(origin);
    different.update(origin);
    CHECK_MESSAGE(same(original, repeat.chunks()), "Same seed produced different terrain");
    CHECK_MESSAGE(!same(original, different.chunks()), "Different seeds produced identical terrain");
    for (int i = 1; i <= 100; ++i) {
        const float x = float(i * 1000);
        terrain.update({{x - 40, -30}, {x + 40, 30}});
        CHECK_MESSAGE(terrain.chunks().size() <= 30, "Terrain accumulated visited chunks");
        CHECK_MESSAGE(!terrain.chunks().contains({0, 0}), "Distant origin chunk retained");
    }
    terrain.update(origin);
    CHECK_MESSAGE(same(original, terrain.chunks()), "Reloading changed terrain");
    // An exact negative boundary must use floor, not truncation toward zero.
    terrain.update({{-32.1f, -0.1f}, {-32.1f, -0.1f}});
    CHECK_MESSAGE((terrain.chunks().size() == 9 && terrain.chunks().contains({-3, -2}) &&
                   terrain.chunks().contains({-1, 0})),
                  "Negative chunk boundary incorrect");
    terrain.update({{-32, 0}, {-32, 0}});
    CHECK_MESSAGE((terrain.chunks().contains({0, 1}) && !terrain.chunks().contains({-3, -2})),
                  "Exact chunk boundary incorrect");
    rejects(terrain, {{1, 0}, {-1, 0}});
    rejects(terrain, {{0, 0}, {std::numeric_limits<float>::infinity(), 1}});
    rejects(terrain, {{std::numeric_limits<float>::quiet_NaN(), 0}, {1, 1}});
    rejects(terrain, {{-200001, 0}, {0, 0}});
    rejects(terrain, {{-2000, -2000}, {2000, 2000}});
}

TEST_CASE("generated obstacles are static and disjoint inside their chunk and outside the spawn clearing") {
    demo::Terrain terrain(736);
    terrain.update({{-160, -160}, {160, 160}});
    std::size_t count = 0;
    for (const auto &[coordinate, bodies] : terrain.chunks()) {
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const auto &body = bodies[i];
            validate(body);
            CHECK_MESSAGE(body.motion == Motion::fixed, "Terrain must be static");
            const auto area = bounds(body);
            CHECK_MESSAGE((area.min.x >= float(coordinate.first * 32) &&
                           area.max.x <= float((coordinate.first + 1) * 32) &&
                           area.min.y >= float(coordinate.second * 32) &&
                           area.max.y <= float((coordinate.second + 1) * 32)),
                          "Obstacle crosses chunk boundary");
            CHECK_MESSAGE((area.max.x < -32 || area.min.x > 32 || area.max.y < -19 || area.min.y > 19),
                          "Obstacle inside spawn clearing");
            for (std::size_t j = 0; j < i; ++j)
                CHECK_MESSAGE(!collide(body, bodies[j]), "Generated obstacles overlap");
            ++count;
        }
    }
    CHECK_MESSAGE(count > 200, "World generated too few obstacles");
}

TEST_CASE("scene streaming keeps the player and gravity and loads physics only near the view") {
    demo::Scene scene(12, 42);
    Camera2D camera;
    scene.stream(camera);
    const auto movers = scene.moving_body_count;
    CHECK_MESSAGE(movers == scene.agents().size() + 7, "Mover population accounting changed");
    auto &player = scene.world.body(scene.player);
    player.position = {2, -3};
    player.velocity = {1, 2};
    player.angle = 0.7f;
    scene.world.gravity = {0, -9.81f};
    scene.target_agents({100, 120});
    scene.update(1.f / 120, {});
    const std::vector<Body> before(scene.world.bodies().begin(), scene.world.bodies().begin() + movers);
    const auto original = scene.terrain.chunks();
    const Body player_before_stream = scene.world.body(scene.player);
    camera.position = {500, -500};
    scene.stream(camera);
    CHECK_MESSAGE(same(player_before_stream, scene.world.body(scene.player)),
                  "Streaming changed player state");
    CHECK_MESSAGE(scene.world.gravity.y == -9.81f, "Streaming reset gravity");
    std::size_t expected = scene.moving_body_count;
    for (const auto &[coordinate, bodies] : scene.terrain.chunks())
        for (const auto &body : bodies) {
            const auto center = scene.world.body(scene.player).position;
            const auto half = camera.viewport * (1.25f / 10.f);
            expected += Aabb{center - half, center + half}.overlaps(bounds(body));
        }
    CHECK_MESSAGE(scene.world.bodies().size() == expected, "Physics retained unloaded obstacles");
    camera.position = {};
    scene.stream(camera);
    for (const auto &[coordinate, bodies] : original)
        if (scene.terrain.chunks().contains(coordinate))
            for (std::size_t i = 0; i < bodies.size(); ++i)
                CHECK_MESSAGE(same(bodies[i], scene.terrain.chunks().at(coordinate)[i]),
                              "Scene return changed obstacles");
    camera.viewport = {1920, 1080};
    camera.pixels_per_unit = 5;
    scene.stream(camera);
    CHECK_MESSAGE(scene.terrain.chunks().size() > original.size(), "Zoom/resize did not load a larger area");
    // The regenerated shapes participate in physics, not just rendering.
    for (const auto &[coordinate, bodies] : scene.terrain.chunks())
        if (!bodies.empty()) {
            scene.world.body(scene.player).position = bodies.front().position;
            scene.world.body(scene.player).velocity = {};
            scene.world.gravity = {};
            const float depth = collide(scene.world.body(scene.player), bodies.front())->penetration;
            scene.world.step(1.f / 120);
            const auto contact = collide(scene.world.body(scene.player), bodies.front());
            CHECK_MESSAGE((!contact || contact->penetration < depth),
                          "Generated obstacle did not resolve collision");
            break;
        }
}
// Wander resumes at the destination, while dormant navigation remains frozen.
TEST_CASE("agents wander on from reached goals and distant patrols are evicted") {
    demo::Scene scene(12, 42);
    // The assertions below are about wander routes; steering wander plans none.
    scene.tuning = route_wander();
    const auto id = scene.agents().front().body_id();
    const Vec2 home = scene.agents().front().home();
    CHECK_MESSAGE((length(scene.agents().front().goal() - home) == 0 && scene.agents().front().wandering() &&
                   scene.agents().front().route().empty() && scene.agents().front().replan_in() == 0 &&
                   !scene.agents().front().active()),
                  "Initial patrol state changed");
    Camera2D camera;
    camera.position = home;
    scene.world.body(scene.player).position = home + Vec2{2, 0};
    scene.stream(camera);
    CHECK_MESSAGE(scene.agents().front().active(), "Home-centered agent should be visible");
    scene.target_agents(home);
    scene.update(1.f / 120, {});
    CHECK_MESSAGE((length(scene.agents().front().goal() - home) >= 2 && scene.agents().front().wandering() &&
                   scene.agents().front().replan_in() == 2),
                  "Reaching an assigned goal must start a reachable wander route");
    const auto previous_path_size = scene.agents().front().route().size();
    scene.target_agents(home + Vec2{8, 8});
    CHECK_MESSAGE((scene.agents().front().replan_in() == 0 &&
                   scene.agents().front().route().size() == previous_path_size),
                  "Retargeting must invalidate the timer without consuming the displayed route");
    scene.world.body(id).position = home + Vec2{8, 8};
    scene.update(1.f / 120, {});
    CHECK_MESSAGE((scene.agents().front().wandering() && length(scene.agents().front().goal() - home) > 1),
                  "Reaching an assigned goal must wander from the arrival location");
    scene.world.body(scene.player).position = {2000, 2000};
    camera.position = {2000, 2000};
    scene.stream(camera);
    for (const auto &agent : scene.agents())
        CHECK_MESSAGE(length(agent.home() - home) > 100, "Distant patrol was not evicted");
}

TEST_CASE("agent navigation stays frozen while dormant and holds position for unreachable goals") {
    kinetra::Navigation navigation;
    navigation.rebuild({{-20, -20}, {20, 20}}, {});
    Body body;
    demo::Agent agent(7, body.position);
    CHECK_MESSAGE((agent.body_id() == 7 && !agent.active()), "Agent identity or initial visibility changed");
    const auto tuning = route_wander();
    CHECK_MESSAGE((length(agent.advance_navigation(0.1f, body, navigation, tuning) - body.position) == 0 &&
                   agent.replan_in() == 0 && agent.route().empty()),
                  "Dormant navigation changed initial state");
    agent.update_visibility({{-20, -20}, {20, 20}}, body);
    agent.retarget({12, 0});
    const Vec2 target = agent.advance_navigation(1.f / 120, body, navigation, tuning);
    CHECK_MESSAGE((target.x > 0 && target.y == 0 && !agent.route().empty() &&
                   length(agent.route().back() - Vec2{12, 0}) == 0),
                  "Active navigation did not consume the reached start waypoint");
    const auto previous_path_size = agent.route().size();
    agent.retarget({100, 100});
    CHECK_MESSAGE((agent.replan_in() == 0 && agent.route().size() == previous_path_size),
                  "Retarget consumed the previous displayed route before replanning");
    CHECK_MESSAGE(
        (length(agent.advance_navigation(1.f / 120, body, navigation, tuning) - body.position) == 0 &&
         agent.route().empty() && agent.replan_in() == 2),
        "An unreachable goal must hold position and retain the retry interval");
    agent.update_visibility({{100, 100}, {120, 120}}, body);
    agent.advance_navigation(0.1f, body, navigation, tuning);
    CHECK_MESSAGE((agent.replan_in() == 2 && length(agent.goal() - Vec2{100, 100}) == 0 &&
                   length(agent.home() - body.position) == 0),
                  "Dormant navigation advanced a timer or changed goal/home");
}

TEST_CASE("seeded wandering picks new reachable destinations and yields to explicit travel") {
    kinetra::Navigation nav;
    nav.rebuild({{-100, -100}, {100, 100}}, {});
    Body body;
    demo::Agent a(0, {}, 123), repeat(0, {}, 123), other(0, {}, 456);
    for (auto *agent : {&a, &repeat, &other})
        agent->update_visibility({{-100, -100}, {100, 100}}, body);
    // The twelve legs below leave the default home range, so the agent roams freely.
    const auto tuning = free_route_wander();
    a.advance_navigation(1.f / 120, body, nav, tuning);
    repeat.advance_navigation(1.f / 120, body, nav, tuning);
    other.advance_navigation(1.f / 120, body, nav, tuning);
    CHECK_MESSAGE((length(a.goal() - repeat.goal()) == 0 && length(a.goal() - other.goal()) > 0.1f),
                  "Wander must be seeded and vary across seeds");
    Vec2 previous{};
    for (int leg = 0; leg < 12; ++leg) {
        body.position = a.goal();
        const auto arrived = body.position;
        a.advance_navigation(1.f / 120, body, nav, tuning);
        CHECK_MESSAGE((a.wandering() && !a.route().empty() && length(a.goal() - arrived) >= 2 &&
                       length(a.goal() - previous) > 0.1f),
                      "Wander stopped or ping-ponged instead of selecting a new reachable destination");
        previous = arrived;
    }
    a.retarget({50, 50});
    a.advance_navigation(1.f / 120, body, nav, tuning);
    CHECK_MESSAGE((!a.wandering() && length(a.goal() - Vec2{50, 50}) == 0),
                  "Explicit travel must take priority over wandering");
    body.position = {50, 50};
    a.advance_navigation(1.f / 120, body, nav, tuning);
    CHECK_MESSAGE((a.wandering() && length(a.goal() - body.position) >= 2),
                  "Arrival must resume wandering around the new location");
    nav.rebuild({{-1, -1}, {1, 1}}, {});
    body.position = {};
    demo::Agent enclosed(1, {});
    enclosed.update_visibility({{-1, -1}, {1, 1}}, body);
    enclosed.advance_navigation(1.f / 120, body, nav, tuning);
    CHECK_MESSAGE((enclosed.route().empty() && enclosed.replan_in() == 2),
                  "Enclosed wander must wait between bounded planning attempts");
}

TEST_CASE("wandering curves before arrival and short timers do not interrupt commanded travel") {
    Navigation navigation;
    navigation.rebuild({{-100, -100}, {100, 100}}, {});
    demo::Agent agent(0, {}, 123);
    Body body;
    body.motion = Motion::controlled;
    agent.update_visibility({{-100, -100}, {100, 100}}, body);
    // Eight seconds of legs lead out of the default home range, so the agent roams freely.
    const auto tuning = free_route_wander();
    Vec2 previous_direction{}, previous_goal{};
    float total_turn = 0;
    int early_goal_changes = 0;
    Steering settings;
    settings.max_speed = 4;
    for (int tick = 0; tick < 960; ++tick) {
        const float previous_distance = length(agent.goal() - body.position);
        const auto target = agent.advance_navigation(1.f / 120, body, navigation, tuning);
        if (tick > 0 && length(agent.goal() - previous_goal) > 0.1f && previous_distance > 2)
            ++early_goal_changes;
        previous_goal = agent.goal();
        body.acceleration = steer(body, target, {}, settings);
        integrate(body, 1.f / 120);
        const auto direction = normalized(body.velocity);
        if (length(body.velocity) > 1 && length(previous_direction) > 0)
            total_turn += std::abs(
                std::atan2(cross(previous_direction, direction), dot(previous_direction, direction)));
        previous_direction = direction;
    }
    CHECK_MESSAGE((early_goal_changes >= 3 && total_turn > 0.5f),
                  "Wander stayed on long straight legs instead of curving before arrival");
    agent.retarget({80, 80});
    for (int tick = 0; tick < 360; ++tick)
        agent.advance_navigation(1.f / 120, body, navigation, tuning);
    CHECK_MESSAGE((!agent.wandering() && length(agent.goal() - Vec2{80, 80}) < 0.001f),
                  "Short wander timers interrupted commanded travel");
}

TEST_CASE("steering wander moves an agent on a curved course without planning a route") {
    // Never rebuilt, so every route query fails: steering wander must not need one.
    const Navigation navigation;
    const demo::Tuning tuning;
    REQUIRE_MESSAGE(tuning.wander.mode == demo::WanderMode::steering, "Steering is not the default");
    demo::Agent agent(0, {}, 123);
    Body body;
    body.motion = Motion::controlled;
    agent.update_visibility(everywhere, body);
    Vec2 previous_direction{};
    float travelled = 0, total_turn = 0;
    for (int tick = 0; tick < 600; ++tick) {
        const Vec2 before = body.position;
        const Vec2 target = agent.advance_navigation(tick_seconds, body, navigation, tuning);
        REQUIRE_MESSAGE((agent.wandering() && agent.steers_without_route() && agent.route().empty()),
                        "Steering wander planned a route");
        REQUIRE_MESSAGE(length(target - agent.goal()) == 0, "The steering target is not reported as goal");
        // The target lies on a circle of half the reach, a full reach ahead.
        const float ahead = length(target - body.position);
        REQUIRE_MESSAGE((ahead >= tuning.wander.reach * 0.499f && ahead <= tuning.wander.reach * 1.501f),
                        "The steering target left the wander circle");
        follow(body, target, tuning);
        travelled += length(body.position - before);
        const Vec2 direction = normalized(body.velocity);
        if (length(body.velocity) > 1 && length(previous_direction) > 0)
            total_turn += std::abs(signed_angle(previous_direction, direction));
        previous_direction = direction;
    }
    // Five seconds at up to 4 units per second.
    CHECK_MESSAGE(travelled > 15, "A steering wanderer did not keep moving");
    CHECK_MESSAGE(total_turn > 0.5f, "A steering wanderer walked a straight line");
}

TEST_CASE("steering wander is repeatable for a seed and differs between seeds") {
    const Navigation navigation;
    const demo::Tuning tuning;
    demo::Agent agent(0, {}, 123), repeat(0, {}, 123), other(0, {}, 456);
    Body body;
    body.motion = Motion::controlled;
    for (auto *each : {&agent, &repeat, &other})
        each->update_visibility(everywhere, body);
    bool differs = false;
    for (int tick = 0; tick < 600; ++tick) {
        const Vec2 target = agent.advance_navigation(tick_seconds, body, navigation, tuning);
        REQUIRE_MESSAGE(length(target - repeat.advance_navigation(tick_seconds, body, navigation, tuning)) ==
                            0,
                        "Two agents with one seed chose different steering targets");
        differs = differs ||
                  length(target - other.advance_navigation(tick_seconds, body, navigation, tuning)) > 0.01f;
        follow(body, target, tuning);
    }
    CHECK_MESSAGE(differs, "Steering wander ignores its seed");
}

TEST_CASE("steering wander returns an agent to its home range and keeps it there") {
    const Navigation navigation;
    const demo::Tuning tuning;
    const Vec2 home{3, -2};
    demo::Agent agent(0, home, 123);
    Body body;
    body.motion = Motion::controlled;
    // Well beyond the home range of 12 units.
    body.position = home + Vec2{30, 0};
    agent.update_visibility(everywhere, body);
    // The 18 units back to the range take 4.5 seconds at top speed; allow 10.
    for (int tick = 0; tick < 1200; ++tick)
        follow(body, agent.advance_navigation(tick_seconds, body, navigation, tuning), tuning);
    CHECK_MESSAGE(distance(body.position, home) < tuning.wander.range,
                  "A steering wanderer did not return to its home range");
    float farthest = 0;
    for (int tick = 0; tick < 7200; ++tick) {
        follow(body, agent.advance_navigation(tick_seconds, body, navigation, tuning), tuning);
        farthest = std::max(farthest, distance(body.position, home));
    }
    INFO("farthest from home in 60 seconds: " << farthest);
    CHECK_MESSAGE(farthest <= tuning.wander.range, "A steering wanderer left its home range in open space");
    CHECK_MESSAGE(farthest > tuning.wander.range / 2, "A steering wanderer stayed at home");
}

TEST_CASE("commanded travel plans a route for a steering wanderer and steering resumes on arrival") {
    Navigation navigation;
    navigation.rebuild({{-20, -20}, {20, 20}}, {});
    const demo::Tuning tuning;
    demo::Agent agent(0, {}, 123);
    Body body;
    agent.update_visibility(everywhere, body);
    agent.advance_navigation(tick_seconds, body, navigation, tuning);
    REQUIRE_MESSAGE(agent.steers_without_route(), "A new agent does not wander by steering");
    agent.retarget({10, 0});
    agent.advance_navigation(tick_seconds, body, navigation, tuning);
    CHECK_MESSAGE((!agent.wandering() && !agent.steers_without_route() && !agent.route().empty() &&
                   length(agent.goal() - Vec2{10, 0}) == 0),
                  "A commanded steering wanderer did not plan a route to its goal");
    body.position = {10, 0};
    agent.advance_navigation(tick_seconds, body, navigation, tuning);
    CHECK_MESSAGE((agent.wandering() && agent.steers_without_route() && agent.route().empty()),
                  "Arrival did not resume steering wander without a route");
    agent.update_visibility({{100, 100}, {120, 120}}, body);
    CHECK_MESSAGE(!agent.steers_without_route(), "A dormant agent still reports steering");
}

TEST_CASE("route wander starts its legs toward home beyond the home range") {
    Navigation navigation;
    navigation.rebuild({{-100, -100}, {100, 100}}, {});
    const auto tuning = route_wander();
    const Vec2 home{};
    demo::Agent agent(0, home, 123);
    Body body;
    body.position = {40, 0};
    agent.update_visibility(everywhere, body);
    // A leg covers at least 2 units toward home, so 28 units take at most 14 legs.
    int legs = 0;
    while (distance(body.position, home) > tuning.wander.range && legs < 14) {
        const float before = distance(body.position, home);
        agent.advance_navigation(tick_seconds, body, navigation, tuning);
        REQUIRE_MESSAGE((agent.wandering() && !agent.route().empty() && !agent.steers_without_route()),
                        "Route wander did not plan a leg");
        REQUIRE_MESSAGE(distance(agent.goal(), home) < before - 2,
                        "A leg beyond the home range did not lead toward home");
        body.position = agent.goal();
        ++legs;
    }
    CHECK_MESSAGE(distance(body.position, home) <= tuning.wander.range,
                  "Route wander did not return to its home range");
}

TEST_CASE("a crowded population spawns visible near the initial camera without overlapping the player") {
    demo::Scene scene(240, 42);
    CHECK_MESSAGE(scene.agents().size() == 240, "Crowded initial population was not created");
    Camera2D camera;
    camera.viewport = {1280, 800};
    camera.pixels_per_unit = 17;
    camera.position = scene.world.body(scene.player).position;
    scene.stream(camera);
    std::size_t visible = 0;
    for (const auto &agent : scene.agents()) {
        const auto &body = scene.world.body(agent.body_id());
        if (scene.view.overlaps(bounds(body)))
            ++visible;
        CHECK_MESSAGE(!collide(body, scene.world.body(scene.player)), "Crowded spawn overlaps player");
    }
    CHECK_MESSAGE(visible >= 200, "Crowded population is not visible near the initial camera");
}

TEST_CASE("navigation rejects invalid areas and routes around walls to reachable endpoints") {
    kinetra::Navigation nav;
    for (const Aabb area : {Aabb{{1, 1}, {-1, -1}}, Aabb{{-1000, -1000}, {1000, 1000}},
                            Aabb{{0, 0}, {std::numeric_limits<float>::infinity(), 1}}}) {
        bool rejected = false;
        try {
            nav.rebuild(area, {});
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        CHECK_MESSAGE(rejected, "Invalid navigation area accepted");
    }
    Body wall;
    wall.motion = Motion::fixed;
    wall.shape = Shape::box;
    wall.position = {0, 0};
    wall.half_extents = {1, 7};
    nav.rebuild({{-20, -20}, {20, 20}}, {wall});
    const auto route = nav.path({-12, 0}, {12, 0});
    CHECK_MESSAGE(!route.empty(), "A* did not find a route around wall");
    Vec2 previous{-12, 0};
    bool detour = false;
    for (auto point : route) {
        detour = detour || std::abs(point.y) >= 7.85f;
        for (int sample = 0; sample <= 20; ++sample) {
            Body probe;
            probe.radius = 0.8f;
            probe.position = previous + (point - previous) * (float(sample) / 20.f);
            CHECK_MESSAGE(!collide(probe, wall), "Route crossed inflated obstacle");
        }
        previous = point;
    }
    CHECK_MESSAGE((detour && length(route.back() - Vec2{12, 0}) < 0.01f), "Wrong route endpoint or detour");
    const auto projected = nav.path({-12, 0}, {0, 0});
    CHECK_MESSAGE((!projected.empty() && nav.clear(projected.back(), projected.back()) &&
                   length(projected.back()) <= 2.01f),
                  "Blocked target did not resolve to closest reachable free space");
    CHECK_MESSAGE(nav.path({-12, 0}, {100, 0}).empty(), "Unloaded target accepted");
    wall.half_extents.y = 30;
    nav.rebuild({{-20, -20}, {20, 20}}, {wall});
    CHECK_MESSAGE(nav.path({-12, 0}, {12, 0}).empty(), "Unreachable target accepted");
    const auto reachable_side = nav.path({-12, 0}, {1.2f, 0});
    CHECK_MESSAGE((!reachable_side.empty() && reachable_side.back().x < -1.85f),
                  "Occupied goal resolved across a disconnected wall");
}

TEST_CASE("teleport walking and camera follow respect free space and manual input") {
    demo::Scene scene(0, 42);
    Camera2D camera;
    scene.stream(camera);
    CHECK_MESSAGE(scene.teleport({0, 0}), "Free teleport rejected");
    CHECK_MESSAGE(!scene.teleport({999, 999}), "Offscreen teleport accepted");
    const auto saved_player = scene.world.body(scene.player);
    CHECK_MESSAGE((!scene.teleport(scene.world.body(scene.player + 1).position) &&
                   same(saved_player, scene.world.body(scene.player))),
                  "Teleport overlap accepted or rejection mutated player");
    CHECK_MESSAGE(scene.walk_to({12, 0}), "Player walk route missing");
    for (int i = 0; i < 1500; ++i)
        scene.update(1.f / 120, {});
    CHECK_MESSAGE(length(scene.world.body(scene.player).position - Vec2{12, 0}) < 0.6f,
                  "Player failed to follow route to destination");
    CHECK_MESSAGE(scene.walk_to({-12, 0}), "Second walk route missing");
    demo::PlayerControl input;
    input.direction = {0, 1};
    scene.update(1.f / 120, input);
    CHECK_MESSAGE((!scene.player_goal && scene.player_path.empty()),
                  "Manual input failed to cancel navigation");
    scene.following = true;
    scene.follow(camera);
    CHECK_MESSAGE(length(camera.position - scene.world.body(scene.player).position) == 0,
                  "Camera not centered");
    CHECK_MESSAGE(
        length(camera.world_to_screen(scene.world.body(scene.player).position) - camera.viewport / 2) < 0.01f,
        "Player is not in screen center");
    scene.world.body(scene.player).velocity = {2, 3};
    CHECK_MESSAGE((scene.teleport({0, 0}) && length(scene.world.body(scene.player).velocity) == 0),
                  "Teleport did not reset momentum");
}

// Exercise the complete fixed-step follower, including geometry shortcuts and arrival.
// Both navigation clients must recover after manual movement or a crowd contact
// pushes their body into the conservative planning margin beside static geometry.
TEST_CASE("the player and agents depart from static geometry they touch") {
    for (bool circle : {false, true}) {
        for (float angle : {0.f, 0.6f}) {
            Body obstacle;
            obstacle.motion = Motion::fixed;
            obstacle.shape = circle ? Shape::circle : Shape::box;
            obstacle.radius = 1;
            obstacle.half_extents = {1, 4};
            obstacle.angle = angle;
            demo::Scene scene(0, 42);
            scene.world.clear();
            Body player;
            player.motion = Motion::controlled;
            player.shape = Shape::box;
            player.half_extents = {0.65f, 0.4f};
            player.angle = angle;
            player.position = rotated({-1.77f, 0}, angle);
            player.friction = 0;
            scene.player = scene.world.add(player);
            scene.moving_body_count = 1;
            scene.world.add(obstacle);
            scene.navigation.rebuild({{-20, -20}, {20, 20}}, {obstacle});
            const auto goal = rotated({8, 0}, angle);
            CHECK_MESSAGE(!scene.navigation.clear(player.position, player.position),
                          "Recovery fixture is outside the planning margin");
            CHECK_MESSAGE(scene.walk_to(goal), "Player touching static geometry cannot plan departure");
            for (int tick = 0; tick < 2000; ++tick)
                scene.update(1.f / 120, {});
            CHECK_MESSAGE(length(scene.world.body(scene.player).position - goal) < 0.45f,
                          "Player stayed stuck at static geometry");
            demo::Agent agent(0, player.position, 42);
            Body walker = player;
            walker.shape = Shape::circle;
            walker.radius = 0.32f;
            walker.position = rotated({-1.34f, 0}, angle);
            agent.update_visibility({{-20, -20}, {20, 20}}, walker);
            agent.retarget(goal);
            agent.advance_navigation(1.f / 120, walker, scene.navigation, route_wander());
            CHECK_MESSAGE(!agent.route().empty(), "Agent beside static geometry cannot plan departure");
            CHECK_MESSAGE(scene.navigation.path(obstacle.position, goal).empty(),
                          "Recovery allowed departure from inside solid geometry");
            World simulation;
            const auto walker_id = simulation.add(walker);
            simulation.add(obstacle);
            bool arrived = false;
            Steering settings;
            settings.max_speed = 4;
            settings.slow_radius = 1;
            settings.stop_radius = 0.08f;
            for (int tick = 0; tick < 2000 && !arrived; ++tick) {
                auto &body = simulation.body(walker_id);
                const auto target =
                    agent.advance_navigation(1.f / 120, body, scene.navigation, route_wander());
                body.acceleration = steer(body, target, {}, settings);
                simulation.step(1.f / 120);
                arrived = length(body.position - goal) < 0.6f;
            }
            CHECK_MESSAGE(arrived, "Agent remained stuck after planning outward departure");
        }
    }
}

TEST_CASE("open-space routes go straight and rotated boxes block only their own shape") {
    kinetra::Navigation nav;
    nav.rebuild({{-20, -20}, {20, 20}}, {});
    const auto direct = nav.path({-10.3f, -8.2f}, {11.2f, 9.1f});
    CHECK_MESSAGE((direct.size() == 1 && length(direct.back() - Vec2{11.2f, 9.1f}) < 0.001f),
                  "Open-space diagonal should go directly to the exact click");
    const Body obstacle = wall_box(0.4f);
    nav.rebuild({{-20, -20}, {20, 20}}, {obstacle});
    CHECK_MESSAGE(nav.clear({3, 6}, {3, 6}), "Rotated box still uses oversized world AABB");
    CHECK_MESSAGE(!nav.clear({-12, 0}, {12, 0}), "Swept shortcut crossed rotated obstacle");
    CHECK_MESSAGE(nav.path({-12, 0}, {3, 6}).size() > 0, "Free click beside rotated obstacle was rejected");
}

TEST_CASE("the player follower detours around a wall and keeps a projected click endpoint") {
    const Body obstacle = wall_box(0);
    demo::Scene scene(0, 42);
    scene.world.clear();
    const Body player = follower_player({-12, 0});
    scene.player = scene.world.add(player);
    scene.moving_body_count = 1;
    scene.world.add(obstacle);
    scene.navigation.rebuild({{-20, -20}, {20, 20}}, {obstacle});
    CHECK_MESSAGE((scene.walk_to({12, 0}) && scene.player_path.size() < 8),
                  "Wall detour retained grid staircase");
    for (int tick = 0; tick < 2400; ++tick) {
        scene.update(1.f / 120, {});
        const auto contact = collide(scene.world.body(scene.player), obstacle);
        CHECK_MESSAGE((!contact || contact->penetration < 0.01f), "Follower cut into wall");
    }
    CHECK_MESSAGE(length(scene.world.body(scene.player).position - Vec2{12, 0}) < 0.45f,
                  "Follower did not arrive around wall");
    REQUIRE_MESSAGE((scene.walk_to({0, 0}) && scene.player_goal && length(*scene.player_goal) < 2.01f),
                    "Player did not retain resolved click endpoint");
    const auto endpoint = *scene.player_goal;
    for (int tick = 0; tick < 2400; ++tick)
        scene.update(1.f / 120, {});
    CHECK_MESSAGE(length(scene.world.body(scene.player).position - endpoint) < 0.45f,
                  "Player did not arrive at projected endpoint");
}

// Agents use the same follower through the actual scene/physics integration.
TEST_CASE("an agent follower arrives around a wall and physically resumes wandering") {
    const Body obstacle = wall_box(0);
    demo::Scene crowd(1, 42);
    // The wall is added by hand below, so only a planned route knows about it: the scene
    // indexes the terrain it streamed in itself for agents that steer without a route.
    crowd.tuning = route_wander();
    Camera2D camera;
    camera.viewport = {8, 8};
    camera.position = crowd.agents().front().home();
    crowd.world.body(crowd.player).position = camera.position;
    crowd.stream(camera);
    crowd.world.clear();
    Body walker;
    walker.position = {-12, 0};
    walker.motion = Motion::controlled;
    walker.radius = 0.32f;
    walker.friction = 0;
    crowd.world.add(walker);
    crowd.player = crowd.world.add(follower_player({-12, -15}));
    crowd.moving_body_count = 2;
    crowd.world.add(obstacle);
    crowd.navigation.rebuild({{-20, -20}, {20, 20}}, {obstacle});
    crowd.target_agents({12, 0});
    bool arrived = false;
    for (int tick = 0; tick < 2400 && !arrived; ++tick) {
        crowd.update(1.f / 120, {});
        const auto &moving = crowd.world.body(0);
        const auto contact = collide(moving, obstacle);
        CHECK_MESSAGE((!contact || contact->penetration < 0.01f), "Agent follower cut into wall");
        arrived = length(moving.position - Vec2{12, 0}) < 0.6f;
    }
    CHECK_MESSAGE(arrived, "Autonomous agent did not arrive around wall");
    bool resumed_wander = false;
    for (int tick = 0; tick < 1200 && !resumed_wander; ++tick) {
        crowd.update(1.f / 120, {});
        const auto &moving = crowd.world.body(0);
        const auto contact = collide(moving, obstacle);
        CHECK_MESSAGE((!contact || contact->penetration < 0.01f), "Resumed wander cut through the wall");
        resumed_wander = crowd.agents().front().wandering() && length(moving.position - Vec2{12, 0}) > 2;
    }
    CHECK_MESSAGE(resumed_wander, "Agent reached its order but did not physically resume wandering");
}

TEST_CASE("circle obstacles keep swept route clearance and occupied goals resolve to reachable endpoints") {
    Body obstacle = wall_box(0);
    obstacle.shape = Shape::circle;
    obstacle.radius = 2;
    kinetra::Navigation nav;
    nav.rebuild({{-20, -20}, {20, 20}}, {obstacle});
    CHECK_MESSAGE(nav.clear({2.2f, 2.2f}, {2.2f, 2.2f}), "Circle clearance used square bounds");
    const auto round_route = nav.path({-8, 0}, {8, 0});
    CHECK_MESSAGE(!round_route.empty(), "Circle detour missing");
    Vec2 previous{-8, 0};
    for (const auto point : round_route) {
        CHECK_MESSAGE(nav.clear(previous, point), "Smoothed circle route lacks swept clearance");
        previous = point;
    }
    demo::Agent agent(0, {-8, 0});
    Body body;
    body.position = {-8, 0};
    agent.update_visibility({{-20, -20}, {20, 20}}, body);
    agent.retarget({0, 0});
    agent.advance_navigation(1.f / 120, body, nav, route_wander());
    CHECK_MESSAGE(
        (!agent.route().empty() && nav.clear(agent.goal(), agent.goal()) && length(agent.goal()) <= 3.01f),
        "Agent did not adopt reachable endpoint for occupied goal");
}

TEST_CASE("a ghost camera neither activates agents nor rejects free teleports") {
    demo::Scene scene(12, 42);
    Camera2D camera;
    scene.stream(camera);
    std::vector<bool> active;
    std::vector<Vec2> goals;
    for (const auto &agent : scene.agents()) {
        active.push_back(agent.active());
        goals.push_back(agent.goal());
    }
    camera.position = {1000, 1000};
    camera.pixels_per_unit = 5;
    scene.stream(camera);
    CHECK_MESSAGE(active.size() == scene.agents().size(), "Ghost camera changed resident population");
    scene.target_agents({1, 2});
    for (std::size_t i = 0; i < active.size(); ++i) {
        const auto &agent = scene.agents()[i];
        CHECK_MESSAGE(agent.active() == active[i], "Ghost camera changed agent activation");
        if (!active[i])
            CHECK_MESSAGE((agent.route().empty() && length(agent.goal() - goals[i]) == 0),
                          "Dormant agent received a path or group goal");
    }
    auto candidate = scene.world.body(scene.player);
    bool free_destination = false;
    for (int x = 0; x < 20 && !free_destination; ++x) {
        candidate.position = camera.position + Vec2{float(x), 0};
        free_destination = !scene.terrain.blocked(candidate);
    }
    CHECK_MESSAGE((free_destination && scene.teleport(candidate.position) &&
                   length(scene.world.body(scene.player).position - candidate.position) == 0),
                  "Ghost teleport rejected a free nonresident destination");
}

TEST_CASE("the population is seeded and dispersed and streams without duplicates or invalid handles") {
    demo::Scene scene(100, 42), repeat(100, 42), different(100, 43);
    bool spread = false, changed = false;
    for (std::size_t i = 0; i < scene.agents().size(); ++i) {
        const auto &body = scene.world.body(scene.agents()[i].body_id());
        CHECK_MESSAGE(same(body, repeat.world.body(repeat.agents()[i].body_id())),
                      "Population seed not repeatable");
        changed = changed || !same(body, different.world.body(different.agents()[i].body_id()));
        spread = spread || length(body.position - Vec2{0, -18}) > 25;
        for (std::size_t j = 0; j < i; ++j)
            CHECK_MESSAGE(!collide(body, scene.world.body(scene.agents()[j].body_id())),
                          "Spawned agents overlap");
    }
    CHECK_MESSAGE((spread && changed), "Population not dispersed or seed independent");
    Camera2D camera;
    scene.stream(camera);
    scene.world.body(scene.player).position = {500, 500};
    camera.position = {500, 500};
    scene.stream(camera);
    CHECK_MESSAGE(!scene.agents().empty(), "Exploration did not generate agents");
    const auto generated_count = scene.agents().size();
    scene.stream(camera);
    CHECK_MESSAGE(scene.agents().size() == generated_count, "Stationary camera duplicated agents");
    for (const auto &agent : scene.agents()) {
        CHECK_MESSAGE(agent.home().x > 400, "Distant agent was retained");
        CHECK_MESSAGE((agent.body_id() < scene.moving_body_count && agent.body_id() != scene.player),
                      "Compaction left an invalid agent handle");
    }
    const auto player_before = scene.world.body(scene.player);
    camera.position = {};
    scene.stream(camera);
    CHECK_MESSAGE(same(player_before, scene.world.body(scene.player)), "Eviction mutated player");
    CHECK_MESSAGE(scene.agents().size() <= demo::Scene::maximum_agents, "Population cap exceeded");
}
TEST_CASE("orders reach only their agents and stalled commands resume wandering") {
    demo::Scene scene(2, 42);
    Camera2D selection_camera;
    selection_camera.viewport = {4000, 4000};
    scene.stream(selection_camera);
    const auto first = scene.agents()[0].home(), second = scene.agents()[1].home();
    CHECK_MESSAGE(scene.select_or_target(first), "Agent click did not select");
    CHECK_MESSAGE(scene.select_or_target({400, 400}), "Selected agent did not accept goal");
    CHECK_MESSAGE((length(scene.agents()[0].goal() - Vec2{400, 400}) == 0 &&
                   length(scene.agents()[1].goal() - second) == 0),
                  "Individual order affected another agent");
    scene.target_agents({500, 500});
    for (const auto &agent : scene.agents())
        CHECK_MESSAGE((!agent.active() || length(agent.goal() - Vec2{500, 500}) == 0),
                      "Group order skipped active agent");
    kinetra::Navigation navigation;
    navigation.rebuild({{-30, -30}, {30, 30}}, {});
    Body body;
    demo::Agent agent(0, {});
    agent.update_visibility({{-30, -30}, {30, 30}}, body);
    agent.retarget({500, 500});
    for (int elapsed = 0; elapsed < 21; ++elapsed)
        agent.advance_navigation(1, body, navigation, route_wander());
    CHECK_MESSAGE((agent.wandering() && !agent.route().empty()), "Stalled command never resumed wandering");
}
