#include "demo/world/scene.h"

#include <stdexcept>
#include <utility>

namespace demo {
namespace {
using kinetra::Body;
using kinetra::Motion;
using kinetra::Shape;
using kinetra::Vec2;

Body box(Vec2 position, Vec2 half_extents, Motion motion = Motion::fixed) {
    Body body;
    body.shape = Shape::box;
    body.motion = motion;
    body.position = position;
    body.half_extents = half_extents;
    return body;
}

Body player_at(Vec2 position) {
    auto player = box(position, {0.65f, 0.4f}, Motion::controlled);
    player.friction = 0;
    return player;
}

void add_gravity_props(std::vector<Body> &movers, std::vector<Body> &walls) {
    // Slanted platforms redirect the falling shapes into different contacts.
    auto left = box({-8, 1}, {5, 0.5f});
    left.angle = -0.22f;
    auto right = box({8, -3}, {5, 0.5f});
    right.angle = 0.22f;
    walls.insert(walls.end(), {left, right});
    for (int i = 0; i < 6; ++i) {
        const Vec2 position{-12.f + 4.8f * static_cast<float>(i), 6.f + static_cast<float>(i % 3)};
        Body prop = box(position, {0.8f, 0.65f}, Motion::dynamic);
        if (i % 3 == 0) {
            prop.shape = Shape::circle;
            prop.radius = 0.85f;
        } else if (i % 3 == 2) {
            prop = kinetra::regular_polygon(5, 0.95f);
            prop.position = position;
        }
        prop.restitution = 0.65f;
        prop.angular_velocity = 0.5f;
        movers.push_back(prop);
    }
}
} // namespace

void Scene::arrange_scenario(std::uint32_t seed) {
    // The frame lies outside the playable rectangle, leaving its full width for routing.
    std::vector walls{box({0, -14.5f}, {23, 0.5f}), box({0, 14.5f}, {23, 0.5f}), box({-22.5f, 0}, {0.5f, 14}),
                      box({22.5f, 0}, {0.5f, 14})};
    std::vector movers{player_at({-17, -9})};
    switch (scenario_) {
    case Scenario::gravity:
        add_gravity_props(movers, walls);
        break;
    case Scenario::maze:
        // Alternating wall gaps force a route to change direction several times.
        walls.insert(walls.end(),
                     {box({-10, -3}, {0.7f, 11}), box({0, 3}, {0.7f, 11}), box({10, -3}, {0.7f, 11})});
        break;
    case Scenario::doorway:
        walls.insert(walls.end(), {box({0, -8.5f}, {0.7f, 5.5f}), box({0, 8.5f}, {0.7f, 5.5f})});
        for (const float side : {-1.f, 1.f})
            for (int row = 0; row < 3; ++row) {
                const Vec2 home{side * (9.f + 2.f * static_cast<float>(row)),
                                1.8f * static_cast<float>(row - 1)};
                Body mover;
                mover.motion = Motion::controlled;
                mover.position = home;
                mover.radius = 0.55f;
                mover.friction = 0;
                agents_.emplace_back(movers.size(), home, seed);
                movers.push_back(mover);
            }
        break;
    default:
        throw std::invalid_argument("Unknown fixed Demo scenario");
    }
    arrange(std::move(movers), 0, std::move(walls), scenario_area);
    for (auto &agent : agents_) {
        // Start both streams with opposing goals, even before the first camera update.
        agent.update_visibility(scenario_area, world_.body(agent.body_id()));
        agent.retarget({-agent.home().x, agent.home().y});
    }
}
} // namespace demo
