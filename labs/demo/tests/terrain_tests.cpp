#include "../include/sandbox_scene.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace kinetra;

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

// Compare all generated shape/material fields rather than object padding bytes.
bool same(const Body &a, const Body &b) {
    return a.shape == b.shape && a.motion == b.motion && a.position.x == b.position.x &&
           a.position.y == b.position.y && a.half_extents.x == b.half_extents.x &&
           a.half_extents.y == b.half_extents.y && a.radius == b.radius && a.angle == b.angle &&
           a.friction == b.friction && a.restitution == b.restitution && a.velocity.x == b.velocity.x &&
           a.velocity.y == b.velocity.y && a.acceleration.x == b.acceleration.x &&
           a.acceleration.y == b.acceleration.y && a.angular_velocity == b.angular_velocity &&
           a.mass == b.mass;
}
bool same(const sandbox::Terrain::Chunks &a, const sandbox::Terrain::Chunks &b) {
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
void rejects(sandbox::Terrain &terrain, Aabb view) {
    const auto before = terrain.chunks();
    bool rejected = false;
    try {
        terrain.update(view);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected && same(before, terrain.chunks()), "Invalid view changed terrain or was accepted");
}

void terrain_lifecycle() {
    sandbox::Terrain terrain(42), repeat(42), different(43);
    const Aabb origin{{-40, -30}, {40, 30}};
    require(terrain.update(origin), "Initial terrain not loaded");
    const auto original = terrain.chunks();
    require(!terrain.update(origin), "Unchanged view rebuilt terrain");
    repeat.update(origin);
    different.update(origin);
    require(same(original, repeat.chunks()), "Same seed produced different terrain");
    require(!same(original, different.chunks()), "Different seeds produced identical terrain");
    for (int i = 1; i <= 100; ++i) {
        const float x = float(i * 1000);
        terrain.update({{x - 40, -30}, {x + 40, 30}});
        require(terrain.chunks().size() <= 30, "Terrain accumulated visited chunks");
        require(!terrain.chunks().contains({0, 0}), "Distant origin chunk retained");
    }
    terrain.update(origin);
    require(same(original, terrain.chunks()), "Reloading changed terrain");
    // An exact negative boundary must use floor, not truncation toward zero.
    terrain.update({{-32.1f, -0.1f}, {-32.1f, -0.1f}});
    require(terrain.chunks().size() == 9 && terrain.chunks().contains({-3, -2}) &&
                terrain.chunks().contains({-1, 0}),
            "Negative chunk boundary incorrect");
    terrain.update({{-32, 0}, {-32, 0}});
    require(terrain.chunks().contains({0, 1}) && !terrain.chunks().contains({-3, -2}),
            "Exact chunk boundary incorrect");
    rejects(terrain, {{1, 0}, {-1, 0}});
    rejects(terrain, {{0, 0}, {std::numeric_limits<float>::infinity(), 1}});
    rejects(terrain, {{std::numeric_limits<float>::quiet_NaN(), 0}, {1, 1}});
    rejects(terrain, {{-200001, 0}, {0, 0}});
    rejects(terrain, {{-2000, -2000}, {2000, 2000}});
}

void obstacle_contracts() {
    sandbox::Terrain terrain(736);
    terrain.update({{-160, -160}, {160, 160}});
    std::size_t count = 0;
    for (const auto &[coordinate, bodies] : terrain.chunks()) {
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const auto &body = bodies[i];
            validate(body);
            require(body.motion == Motion::fixed, "Terrain must be static");
            const auto area = bounds(body);
            require(area.min.x >= coordinate.first * 32 && area.max.x <= (coordinate.first + 1) * 32 &&
                        area.min.y >= coordinate.second * 32 && area.max.y <= (coordinate.second + 1) * 32,
                    "Obstacle crosses chunk boundary");
            require(area.max.x < -32 || area.min.x > 32 || area.max.y < -19 || area.min.y > 19,
                    "Obstacle inside spawn clearing");
            for (std::size_t j = 0; j < i; ++j)
                require(!collide(body, bodies[j]), "Generated obstacles overlap");
            ++count;
        }
    }
    require(count > 200, "World generated too few obstacles");
}

void scene_streaming() {
    sandbox::Sandbox scene(12, 42);
    Camera2D camera;
    scene.stream(camera);
    const auto movers = scene.moving_body_count;
    require(movers == scene.agents().size() + 7, "Mover population accounting changed");
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
    require(same(player_before_stream, scene.world.body(scene.player)), "Streaming changed player state");
    require(scene.world.gravity.y == -9.81f, "Streaming reset gravity");
    std::size_t expected = scene.moving_body_count;
    for (const auto &[coordinate, bodies] : scene.terrain.chunks())
        for (const auto &body : bodies) {
            const auto center = scene.world.body(scene.player).position;
            const auto half = camera.viewport * (1.25f / 10.f);
            expected += Aabb{center - half, center + half}.overlaps(bounds(body));
        }
    require(scene.world.bodies().size() == expected, "Physics retained unloaded obstacles");
    camera.position = {};
    scene.stream(camera);
    for (const auto &[coordinate, bodies] : original)
        if (scene.terrain.chunks().contains(coordinate))
            for (std::size_t i = 0; i < bodies.size(); ++i)
                require(same(bodies[i], scene.terrain.chunks().at(coordinate)[i]),
                        "Scene return changed obstacles");
    camera.viewport = {1920, 1080};
    camera.pixels_per_unit = 5;
    scene.stream(camera);
    require(scene.terrain.chunks().size() > original.size(), "Zoom/resize did not load a larger area");
    // The regenerated shapes participate in physics, not just rendering.
    for (const auto &[coordinate, bodies] : scene.terrain.chunks())
        if (!bodies.empty()) {
            scene.world.body(scene.player).position = bodies.front().position;
            scene.world.body(scene.player).velocity = {};
            scene.world.gravity = {};
            const float depth = collide(scene.world.body(scene.player), bodies.front())->penetration;
            scene.world.step(1.f / 120);
            const auto contact = collide(scene.world.body(scene.player), bodies.front());
            require(!contact || contact->penetration < depth, "Generated obstacle did not resolve collision");
            break;
        }
}
// Wander resumes at the destination, while dormant navigation remains frozen.
void agent_lifecycle() {
    sandbox::Sandbox scene(12, 42);
    const auto id = scene.agents().front().body_id();
    const Vec2 home = scene.agents().front().home();
    require(length(scene.agents().front().goal() - home) == 0 && scene.agents().front().wandering() &&
                scene.agents().front().route().empty() && scene.agents().front().replan_in() == 0 &&
                !scene.agents().front().active(),
            "Initial patrol state changed");
    Camera2D camera;
    camera.position = home;
    scene.world.body(scene.player).position = home + Vec2{2, 0};
    scene.stream(camera);
    require(scene.agents().front().active(), "Home-centered agent should be visible");
    scene.target_agents(home);
    scene.update(1.f / 120, {});
    require(length(scene.agents().front().goal() - home) >= 2 && scene.agents().front().wandering() &&
                scene.agents().front().replan_in() == 2,
            "Reaching an assigned goal must start a reachable wander route");
    const auto previous_path_size = scene.agents().front().route().size();
    scene.target_agents(home + Vec2{8, 8});
    require(scene.agents().front().replan_in() == 0 &&
                scene.agents().front().route().size() == previous_path_size,
            "Retargeting must invalidate the timer without consuming the displayed route");
    scene.world.body(id).position = home + Vec2{8, 8};
    scene.update(1.f / 120, {});
    require(scene.agents().front().wandering() && length(scene.agents().front().goal() - home) > 1,
            "Reaching an assigned goal must wander from the arrival location");
    scene.world.body(scene.player).position = {2000, 2000};
    camera.position = {2000, 2000};
    scene.stream(camera);
    for (const auto &agent : scene.agents())
        require(length(agent.home() - home) > 100, "Distant patrol was not evicted");
}

void agent_navigation_contracts() {
    kinetra::Navigation navigation;
    navigation.rebuild({{-20, -20}, {20, 20}}, {});
    Body body;
    sandbox::Agent agent(7, body.position);
    require(agent.body_id() == 7 && !agent.active(), "Agent identity or initial visibility changed");
    require(length(agent.advance_navigation(0.1f, body, navigation) - body.position) == 0 &&
                agent.replan_in() == 0 && agent.route().empty(),
            "Dormant navigation changed initial state");
    agent.update_visibility({{-20, -20}, {20, 20}}, body);
    agent.retarget({12, 0});
    const Vec2 target = agent.advance_navigation(1.f / 120, body, navigation);
    require(target.x > 0 && target.y == 0 && !agent.route().empty() &&
                length(agent.route().back() - Vec2{12, 0}) == 0,
            "Active navigation did not consume the reached start waypoint");
    const auto previous_path_size = agent.route().size();
    agent.retarget({100, 100});
    require(agent.replan_in() == 0 && agent.route().size() == previous_path_size,
            "Retarget consumed the previous displayed route before replanning");
    require(length(agent.advance_navigation(1.f / 120, body, navigation) - body.position) == 0 &&
                agent.route().empty() && agent.replan_in() == 2,
            "An unreachable goal must hold position and retain the retry interval");
    agent.update_visibility({{100, 100}, {120, 120}}, body);
    agent.advance_navigation(0.1f, body, navigation);
    require(agent.replan_in() == 2 && length(agent.goal() - Vec2{100, 100}) == 0 &&
                length(agent.home() - body.position) == 0,
            "Dormant navigation advanced a timer or changed goal/home");
}

void random_wander() {
    kinetra::Navigation nav;
    nav.rebuild({{-100, -100}, {100, 100}}, {});
    Body body;
    sandbox::Agent a(0, {}, 123), repeat(0, {}, 123), other(0, {}, 456);
    for (auto *agent : {&a, &repeat, &other})
        agent->update_visibility({{-100, -100}, {100, 100}}, body);
    a.advance_navigation(1.f / 120, body, nav);
    repeat.advance_navigation(1.f / 120, body, nav);
    other.advance_navigation(1.f / 120, body, nav);
    require(length(a.goal() - repeat.goal()) == 0 && length(a.goal() - other.goal()) > 0.1f,
            "Wander must be seeded and vary across seeds");
    Vec2 previous{};
    for (int leg = 0; leg < 12; ++leg) {
        body.position = a.goal();
        const auto arrived = body.position;
        a.advance_navigation(1.f / 120, body, nav);
        require(a.wandering() && !a.route().empty() && length(a.goal() - arrived) >= 2 &&
                    length(a.goal() - previous) > 0.1f,
                "Wander stopped or ping-ponged instead of selecting a new reachable destination");
        previous = arrived;
    }
    a.retarget({50, 50});
    a.advance_navigation(1.f / 120, body, nav);
    require(!a.wandering() && length(a.goal() - Vec2{50, 50}) == 0,
            "Explicit travel must take priority over wandering");
    body.position = {50, 50};
    a.advance_navigation(1.f / 120, body, nav);
    require(a.wandering() && length(a.goal() - body.position) >= 2,
            "Arrival must resume wandering around the new location");
    nav.rebuild({{-1, -1}, {1, 1}}, {});
    body.position = {};
    sandbox::Agent enclosed(1, {});
    enclosed.update_visibility({{-1, -1}, {1, 1}}, body);
    enclosed.advance_navigation(1.f / 120, body, nav);
    require(enclosed.route().empty() && enclosed.replan_in() == 2,
            "Enclosed wander must wait between bounded planning attempts");
}

void navigation_and_controls() {
    kinetra::Navigation nav;
    for (const Aabb area : {Aabb{{1, 1}, {-1, -1}}, Aabb{{-1000, -1000}, {1000, 1000}},
                            Aabb{{0, 0}, {std::numeric_limits<float>::infinity(), 1}}}) {
        bool rejected = false;
        try {
            nav.rebuild(area, {});
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        require(rejected, "Invalid navigation area accepted");
    }
    Body wall;
    wall.motion = Motion::fixed;
    wall.shape = Shape::box;
    wall.position = {0, 0};
    wall.half_extents = {1, 7};
    nav.rebuild({{-20, -20}, {20, 20}}, {wall});
    const auto route = nav.path({-12, 0}, {12, 0});
    require(!route.empty(), "A* did not find a route around wall");
    Vec2 previous{-12, 0};
    bool detour = false;
    for (auto point : route) {
        detour = detour || std::abs(point.y) >= 7.85f;
        for (int sample = 0; sample <= 20; ++sample) {
            Body probe;
            probe.radius = 0.8f;
            probe.position = previous + (point - previous) * (sample / 20.f);
            require(!collide(probe, wall), "Route crossed inflated obstacle");
        }
        previous = point;
    }
    require(detour && length(route.back() - Vec2{12, 0}) < 0.01f, "Wrong route endpoint or detour");
    const auto projected = nav.path({-12, 0}, {0, 0});
    require(!projected.empty() && nav.clear(projected.back(), projected.back()) &&
                length(projected.back()) <= 2.01f,
            "Blocked target did not resolve to closest reachable free space");
    require(nav.path({-12, 0}, {100, 0}).empty(), "Unloaded target accepted");
    wall.half_extents.y = 30;
    nav.rebuild({{-20, -20}, {20, 20}}, {wall});
    require(nav.path({-12, 0}, {12, 0}).empty(), "Unreachable target accepted");
    const auto reachable_side = nav.path({-12, 0}, {1.2f, 0});
    require(!reachable_side.empty() && reachable_side.back().x < -1.85f,
            "Occupied goal resolved across a disconnected wall");

    sandbox::Sandbox scene(0, 42);
    Camera2D camera;
    scene.stream(camera);
    require(scene.teleport({0, 0}), "Free teleport rejected");
    require(!scene.teleport({999, 999}), "Offscreen teleport accepted");
    const auto saved_player = scene.world.body(scene.player);
    require(!scene.teleport(scene.world.body(scene.player + 1).position) &&
                same(saved_player, scene.world.body(scene.player)),
            "Teleport overlap accepted or rejection mutated player");
    require(scene.walk_to({12, 0}), "Player walk route missing");
    for (int i = 0; i < 1500; ++i)
        scene.update(1.f / 120, {});
    require(length(scene.world.body(scene.player).position - Vec2{12, 0}) < 0.6f,
            "Player failed to follow route to destination");
    require(scene.walk_to({-12, 0}), "Second walk route missing");
    sandbox::PlayerControl input;
    input.direction = {0, 1};
    scene.update(1.f / 120, input);
    require(!scene.player_goal && scene.player_path.empty(), "Manual input failed to cancel navigation");
    scene.following = true;
    scene.follow(camera);
    require(length(camera.position - scene.world.body(scene.player).position) == 0, "Camera not centered");
    require(length(camera.world_to_screen(scene.world.body(scene.player).position) - camera.viewport / 2) <
                0.01f,
            "Player is not in screen center");
    scene.world.body(scene.player).velocity = {2, 3};
    require(scene.teleport({0, 0}) && length(scene.world.body(scene.player).velocity) == 0,
            "Teleport did not reset momentum");
}

// Exercise the complete fixed-step follower, including geometry shortcuts and arrival.
// Both navigation clients must recover after manual movement or a crowd contact
// pushes their body into the conservative planning margin beside static geometry.
void contact_navigation_recovery() {
    for (bool circle : {false, true}) {
        for (float angle : {0.f, 0.6f}) {
            Body obstacle;
            obstacle.motion = Motion::fixed;
            obstacle.shape = circle ? Shape::circle : Shape::box;
            obstacle.radius = 1;
            obstacle.half_extents = {1, 4};
            obstacle.angle = angle;
            sandbox::Sandbox scene(0, 42);
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
            require(!scene.navigation.clear(player.position, player.position),
                    "Recovery fixture is outside the planning margin");
            require(scene.walk_to(goal), "Player touching static geometry cannot plan departure");
            for (int tick = 0; tick < 2000; ++tick)
                scene.update(1.f / 120, {});
            require(length(scene.world.body(scene.player).position - goal) < 0.45f,
                    "Player stayed stuck at static geometry");
            sandbox::Agent agent(0, player.position, 42);
            Body walker = player;
            walker.shape = Shape::circle;
            walker.radius = 0.32f;
            walker.position = rotated({-1.34f, 0}, angle);
            agent.update_visibility({{-20, -20}, {20, 20}}, walker);
            agent.retarget(goal);
            agent.advance_navigation(1.f / 120, walker, scene.navigation);
            require(!agent.route().empty(), "Agent beside static geometry cannot plan departure");
            require(scene.navigation.path(obstacle.position, goal).empty(),
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
                const auto target = agent.advance_navigation(1.f / 120, body, scene.navigation);
                body.acceleration = steer(body, target, {}, settings);
                simulation.step(1.f / 120);
                arrived = length(body.position - goal) < 0.6f;
            }
            require(arrived, "Agent remained stuck after planning outward departure");
        }
    }
}

void smooth_navigation() {
    kinetra::Navigation nav;
    nav.rebuild({{-20, -20}, {20, 20}}, {});
    const auto direct = nav.path({-10.3f, -8.2f}, {11.2f, 9.1f});
    require(direct.size() == 1 && length(direct.back() - Vec2{11.2f, 9.1f}) < 0.001f,
            "Open-space diagonal should go directly to the exact click");
    Body obstacle;
    obstacle.motion = Motion::fixed;
    obstacle.shape = Shape::box;
    obstacle.half_extents = {1, 7};
    obstacle.angle = 0.4f;
    nav.rebuild({{-20, -20}, {20, 20}}, {obstacle});
    require(nav.clear({3, 6}, {3, 6}), "Rotated box still uses oversized world AABB");
    require(!nav.clear({-12, 0}, {12, 0}), "Swept shortcut crossed rotated obstacle");
    require(nav.path({-12, 0}, {3, 6}).size() > 0, "Free click beside rotated obstacle was rejected");
    obstacle.angle = 0;
    sandbox::Sandbox scene(0, 42);
    Camera2D camera;
    scene.world.clear();
    Body player;
    player.motion = Motion::controlled;
    player.shape = Shape::box;
    player.half_extents = {0.65f, 0.4f};
    player.position = {-12, 0};
    player.friction = 0;
    scene.player = scene.world.add(player);
    scene.moving_body_count = 1;
    scene.world.add(obstacle);
    scene.navigation.rebuild({{-20, -20}, {20, 20}}, {obstacle});
    require(scene.walk_to({12, 0}) && scene.player_path.size() < 8, "Wall detour retained grid staircase");
    for (int tick = 0; tick < 2400; ++tick) {
        scene.update(1.f / 120, {});
        const auto contact = collide(scene.world.body(scene.player), obstacle);
        require(!contact || contact->penetration < 0.01f, "Follower cut into wall");
    }
    require(length(scene.world.body(scene.player).position - Vec2{12, 0}) < 0.45f,
            "Follower did not arrive around wall");
    require(scene.walk_to({0, 0}) && scene.player_goal && length(*scene.player_goal) < 2.01f,
            "Player did not retain resolved click endpoint");
    const auto endpoint = *scene.player_goal;
    for (int tick = 0; tick < 2400; ++tick)
        scene.update(1.f / 120, {});
    require(length(scene.world.body(scene.player).position - endpoint) < 0.45f,
            "Player did not arrive at projected endpoint");

    // Agents use the same follower through the actual scene/physics integration.
    sandbox::Sandbox crowd(1, 42);
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
    player.position = {-12, -15};
    crowd.player = crowd.world.add(player);
    crowd.moving_body_count = 2;
    crowd.world.add(obstacle);
    crowd.navigation.rebuild({{-20, -20}, {20, 20}}, {obstacle});
    crowd.target_agents({12, 0});
    bool arrived = false;
    for (int tick = 0; tick < 2400 && !arrived; ++tick) {
        crowd.update(1.f / 120, {});
        const auto &moving = crowd.world.body(0);
        const auto contact = collide(moving, obstacle);
        require(!contact || contact->penetration < 0.01f, "Agent follower cut into wall");
        arrived = length(moving.position - Vec2{12, 0}) < 0.6f;
    }
    require(arrived, "Autonomous agent did not arrive around wall");
    bool resumed_wander = false;
    for (int tick = 0; tick < 1200 && !resumed_wander; ++tick) {
        crowd.update(1.f / 120, {});
        const auto &moving = crowd.world.body(0);
        const auto contact = collide(moving, obstacle);
        require(!contact || contact->penetration < 0.01f, "Resumed wander cut through the wall");
        resumed_wander = crowd.agents().front().wandering() && length(moving.position - Vec2{12, 0}) > 2;
    }
    require(resumed_wander, "Agent reached its order but did not physically resume wandering");

    obstacle.shape = Shape::circle;
    obstacle.radius = 2;
    nav.rebuild({{-20, -20}, {20, 20}}, {obstacle});
    require(nav.clear({2.2f, 2.2f}, {2.2f, 2.2f}), "Circle clearance used square bounds");
    const auto round_route = nav.path({-8, 0}, {8, 0});
    require(!round_route.empty(), "Circle detour missing");
    Vec2 previous{-8, 0};
    for (const auto point : round_route) {
        require(nav.clear(previous, point), "Smoothed circle route lacks swept clearance");
        previous = point;
    }
    sandbox::Agent agent(0, {-8, 0});
    Body body;
    body.position = {-8, 0};
    agent.update_visibility({{-20, -20}, {20, 20}}, body);
    agent.retarget({0, 0});
    agent.advance_navigation(1.f / 120, body, nav);
    require(!agent.route().empty() && nav.clear(agent.goal(), agent.goal()) && length(agent.goal()) <= 3.01f,
            "Agent did not adopt reachable endpoint for occupied goal");
}

void ghost_does_not_activate_agents() {
    sandbox::Sandbox scene(12, 42);
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
    require(active.size() == scene.agents().size(), "Ghost camera changed resident population");
    scene.target_agents({1, 2});
    for (std::size_t i = 0; i < active.size(); ++i) {
        const auto &agent = scene.agents()[i];
        require(agent.active() == active[i], "Ghost camera changed agent activation");
        if (!active[i])
            require(agent.route().empty() && length(agent.goal() - goals[i]) == 0,
                    "Dormant agent received a path or group goal");
    }
    auto candidate = scene.world.body(scene.player);
    bool free_destination = false;
    for (int x = 0; x < 20 && !free_destination; ++x) {
        candidate.position = camera.position + Vec2{float(x), 0};
        free_destination = !scene.terrain.blocked(candidate);
    }
    require(free_destination && scene.teleport(candidate.position) &&
                length(scene.world.body(scene.player).position - candidate.position) == 0,
            "Ghost teleport rejected a free nonresident destination");
}

void population_lifecycle() {
    sandbox::Sandbox scene(100, 42), repeat(100, 42), different(100, 43);
    bool spread = false, changed = false;
    for (std::size_t i = 0; i < scene.agents().size(); ++i) {
        const auto &body = scene.world.body(scene.agents()[i].body_id());
        require(same(body, repeat.world.body(repeat.agents()[i].body_id())),
                "Population seed not repeatable");
        changed = changed || !same(body, different.world.body(different.agents()[i].body_id()));
        spread = spread || length(body.position) > 60;
        for (std::size_t j = 0; j < i; ++j)
            require(!collide(body, scene.world.body(scene.agents()[j].body_id())), "Spawned agents overlap");
    }
    require(spread && changed, "Population not dispersed or seed independent");
    Camera2D camera;
    scene.stream(camera);
    scene.world.body(scene.player).position = {500, 500};
    camera.position = {500, 500};
    scene.stream(camera);
    require(!scene.agents().empty(), "Exploration did not generate agents");
    const auto generated_count = scene.agents().size();
    scene.stream(camera);
    require(scene.agents().size() == generated_count, "Stationary camera duplicated agents");
    for (const auto &agent : scene.agents()) {
        require(agent.home().x > 400, "Distant agent was retained");
        require(agent.body_id() < scene.moving_body_count && agent.body_id() != scene.player,
                "Compaction left an invalid agent handle");
    }
    const auto player_before = scene.world.body(scene.player);
    camera.position = {};
    scene.stream(camera);
    require(same(player_before, scene.world.body(scene.player)), "Eviction mutated player");
    require(scene.agents().size() <= sandbox::Sandbox::maximum_agents, "Population cap exceeded");
}
void selection_and_command_timeout() {
    sandbox::Sandbox scene(2, 42);
    Camera2D selection_camera;
    selection_camera.viewport = {4000, 4000};
    scene.stream(selection_camera);
    const auto first = scene.agents()[0].home(), second = scene.agents()[1].home();
    require(scene.select_or_target(first), "Agent click did not select");
    require(scene.select_or_target({400, 400}), "Selected agent did not accept goal");
    require(length(scene.agents()[0].goal() - Vec2{400, 400}) == 0 &&
                length(scene.agents()[1].goal() - second) == 0,
            "Individual order affected another agent");
    scene.target_agents({500, 500});
    for (const auto &agent : scene.agents())
        require(!agent.active() || length(agent.goal() - Vec2{500, 500}) == 0,
                "Group order skipped active agent");
    kinetra::Navigation navigation;
    navigation.rebuild({{-30, -30}, {30, 30}}, {});
    Body body;
    sandbox::Agent agent(0, {});
    agent.update_visibility({{-30, -30}, {30, 30}}, body);
    agent.retarget({500, 500});
    for (int second = 0; second < 21; ++second)
        agent.advance_navigation(1, body, navigation);
    require(agent.wandering() && !agent.route().empty(), "Stalled command never resumed wandering");
}

} // namespace

int main() {
    try {
        terrain_lifecycle();
        obstacle_contracts();
        scene_streaming();
        agent_lifecycle();
        random_wander();
        agent_navigation_contracts();
        navigation_and_controls();
        contact_navigation_recovery();
        smooth_navigation();
        ghost_does_not_activate_agents();
        population_lifecycle();
        selection_and_command_timeout();
        std::cout << "PASS terrain seeds, unload/reload, bounds, obstacles and scene integration\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
