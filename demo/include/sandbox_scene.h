#pragma once

#include "agent.h"
#include "player_control.h"
#include "terrain.h"
#include <kinetra/camera.h>
#include <kinetra/steering.h>
#include <optional>
#include <set>

namespace sandbox {
using namespace kinetra;

/// Application-owned population, routes and terrain. Eviction remaps body indices.
class Sandbox {
  public:
    World world;
    Terrain terrain;
    Terrain preview;
    kinetra::Navigation navigation;
    std::size_t moving_body_count{};
    std::size_t player{};
    std::vector<Vec2> player_path;
    std::optional<Vec2> player_goal;
    std::optional<Vec2> selected_agent;
    float player_replan{};
    bool following{true};
    Aabb view{};
    static constexpr std::size_t maximum_agents = 1200;

    explicit Sandbox(int count, std::uint32_t seed) : terrain(seed), preview(seed) {
        if (count < 0 || count > 300)
            throw std::invalid_argument("Initial agent count must be 0..300");
        // Use the world seed for dispersed, repeatable spawn candidates. Temporary
        // terrain covers the spawn area so agents cannot start inside obstacles.
        Terrain spawn_terrain(seed);
        spawn_terrain.update({{-96, -96}, {96, 96}});
        std::mt19937 random(seed);
        for (int i = 0; i < count; ++i) {
            bool placed = false;
            for (int attempt = 0; attempt < 1000 && !placed; ++attempt) {
                const auto unit = [&]() { return float(random() >> 8) / 16777216.f; };
                const Vec2 position{-90 + unit() * 180, -90 + unit() * 180};
                placed = spawn(position, spawn_terrain.chunks());
            }
            if (!placed)
                throw std::runtime_error("Could not place initial population safely");
        }
        Body b;
        b.motion = Motion::controlled;
        b.shape = Shape::box;
        b.position = {0, -18};
        b.half_extents = {0.65f, 0.4f};
        b.friction = 0;
        player = world.add(b);
        for (int i = 0; i < 6; ++i) {
            Body ball;
            ball.shape = i % 2 ? Shape::circle : Shape::box;
            ball.position = {-22.f + i * 8, 16};
            ball.velocity = {i % 2 ? -4.f : 4.f, -1};
            ball.angle = 0.35f * i;
            ball.angular_velocity = 0.7f;
            ball.restitution = 0.9f;
            world.add(ball);
        }
        moving_body_count = world.bodies().size();
        // Initial agents already populate these chunks; exploration adds agents elsewhere.
        for (int y = -3; count > 0 && y <= 2; ++y)
            for (int x = -3; x <= 2; ++x)
                populated_.emplace(x, y);
    }

    /// Read-only population view. Streaming may invalidate references and remap body IDs.
    std::span<const Agent> agents() const { return agents_; }

    /// Retain player-centered terrain and populate newly active chunks once. The population
    /// is capped; distant agents are evicted and revisits can repopulate chunks.
    void stream(const Camera2D &camera) {
        camera.validate();
        const Vec2 half_view = camera.viewport / (2 * camera.pixels_per_unit);
        view = {camera.position - half_view, camera.position + half_view};
        if (!following)
            preview.update(view, 0);
        bool removed = false;
        std::vector<bool> discard(moving_body_count);
        // Lifecycle distances are player-centered and independent of Ghost and current zoom.
        const auto center = world.body(player).position;
        const Vec2 active_half = camera.viewport * (1.75f / (2 * 17.f));
        const Aabb active_view{center - active_half, center + active_half};
        const Vec2 retained_half = camera.viewport * (1.25f / (2 * 5.f));
        const Aabb retained{center - retained_half, center + retained_half};
        for (std::size_t id = 0; id < moving_body_count; ++id)
            if (id != player && !retained.overlaps(bounds(world.body(id)))) {
                discard[id] = true;
                removed = true;
            }
        if (removed) {
            // Compact movers and remap every handle atomically before rebuilding terrain.
            std::vector<std::size_t> remap(moving_body_count);
            std::vector<Body> retained_bodies;
            for (std::size_t i = 0; i < moving_body_count; ++i)
                if (!discard[i]) {
                    remap[i] = retained_bodies.size();
                    retained_bodies.push_back(world.body(i));
                }
            std::erase_if(agents_, [&](const Agent &a) { return discard[a.body_id()]; });
            for (auto &agent : agents_)
                agent.remap_body(remap[agent.body_id()]);
            player = remap[player];
            world.clear();
            for (const auto &body : retained_bodies)
                world.add(body);
            moving_body_count = retained_bodies.size();
        }
        // Remember only resident chunks, allowing bounded state and fresh populations on return.
        std::erase_if(populated_, [&](const auto &coordinate) {
            const Vec2 corner{coordinate.first * 32.f, coordinate.second * 32.f};
            return !retained.overlaps({corner, corner + Vec2{32, 32}});
        });
        const bool changed = terrain.update(retained, 0) || removed;
        std::vector<Body> movers(world.bodies().begin(), world.bodies().begin() + moving_body_count);
        if (changed) {
            world.clear();
            for (const auto &body : movers)
                world.add(body);
        }
        bool added = false;
        for (const auto &[coordinate, obstacles] : terrain.chunks()) {
            const Vec2 corner{coordinate.first * Terrain::chunk_size,
                              coordinate.second * Terrain::chunk_size};
            if (!active_view.overlaps({corner, corner + Vec2{32, 32}}) || agents_.size() >= maximum_agents ||
                !populated_.insert(coordinate).second)
                continue;
            std::seed_seq sequence{terrain.seed(), std::uint32_t(coordinate.first),
                                   std::uint32_t(coordinate.second), 12345u};
            std::mt19937 random(sequence);
            // Remove disposable obstacles before inserting movers to retain stable indices.
            if (!changed && !added) {
                world.clear();
                for (const auto &body : movers)
                    world.add(body);
            }
            added = true;
            for (int attempt = 0, made = 0; attempt < 40 && made < 3 && agents_.size() < maximum_agents;
                 ++attempt) {
                const auto unit = [&]() { return float(random() >> 8) / 16777216.f; };
                if (spawn(corner + Vec2{2 + unit() * 28, 2 + unit() * 28}, terrain.chunks()))
                    ++made;
            }
        }
        if (changed || added) {
            moving_body_count = world.bodies().size();
            std::vector<Body> obstacles;
            for (const auto &[coordinate, bodies] : terrain.chunks())
                for (const auto &body : bodies) {
                    if (!retained.overlaps(bounds(body)))
                        continue;
                    world.add(body);
                    obstacles.push_back(body);
                }
            // Plan only around simulated actors, rather than allocating navigation
            // for the much larger debug residency area. Margin covers wander goals.
            const Vec2 route_margin{24, 24};
            navigation.rebuild({active_view.min - route_margin, active_view.max + route_margin}, obstacles);
            for (auto &agent : agents_)
                agent.invalidate_route();
            player_replan = 0;
        }
        for (auto &agent : agents_)
            agent.update_visibility(active_view, world.body(agent.body_id()));
    }

    /// Teleport only into free loaded space and discard previous navigation/momentum.
    bool teleport(Vec2 destination) {
        if (!finite(destination) || !view.overlaps({destination, destination}))
            return false;
        Body candidate = world.body(player);
        candidate.position = destination;
        if (terrain.blocked(candidate))
            return false;
        for (std::size_t i = 0; i < world.bodies().size(); ++i)
            if (i != player && collide(candidate, world.body(i)))
                return false;
        candidate.velocity = candidate.acceleration = {};
        candidate.angular_velocity = 0;
        world.body(player) = candidate;
        player_goal.reset();
        player_path.clear();
        return true;
    }

    bool walk_to(Vec2 destination) {
        player_path = navigation.path(world.body(player).position, destination);
        player_goal = player_path.empty() ? std::nullopt : std::optional<Vec2>(player_path.back());
        player_replan = 2;
        return player_goal.has_value();
    }

    bool select_or_target(Vec2 destination) {
        for (auto &agent : agents_)
            if (agent.active() && length(world.body(agent.body_id()).position - destination) < 1.f) {
                if (selected_agent && length(*selected_agent - agent.home()) < 0.001f)
                    selected_agent.reset();
                else
                    selected_agent = agent.home();
                return true;
            }
        if (selected_agent) {
            for (auto &agent : agents_)
                if (length(agent.home() - *selected_agent) < 0.001f) {
                    agent.retarget(destination);
                    return true;
                }
            selected_agent.reset();
        }
        return false;
    }

    void target_agents(Vec2 destination) {
        selected_agent.reset();
        for (auto &agent : agents_)
            agent.retarget(destination);
    }

    /// Follow the latest player pose, including while paused or after teleportation.
    void follow(Camera2D &camera) const {
        if (following)
            camera.position = world.body(player).position;
    }

    /// Apply player intent and active-agent steering for one fixed tick; sleeping
    /// agents remain collision obstacles and regain their exact state after physics.
    void update(float dt, const PlayerControl &control) {
        std::vector<Aabb> boxes;
        for (std::size_t id = 0; id < moving_body_count; ++id)
            boxes.push_back(bounds(world.body(id)));
        SpatialGrid neighbors(4);
        neighbors.rebuild(boxes);
        Steering steering;
        steering.max_speed = 4;
        steering.lookahead = 0.8f;
        steering.slow_radius = 1;
        steering.stop_radius = 0.08f;
        steering.clearance = 0.05f;
        const auto acceleration = [&](std::size_t id, Vec2 target) {
            const auto &body = world.body(id);
            std::vector<Body> obstacles;
            const Vec2 reach{6, 6};
            for (auto other : neighbors.query({body.position - reach, body.position + reach}))
                // Terrain clearance is handled by the swept route. Local avoidance
                // handles movers without fighting planned static-obstacle corners.
                if (other != id && other < moving_body_count)
                    obstacles.push_back(world.body(other));
            auto settings = steering;
            if (id != player) {
                settings.separation_radius = 1.6f;
                settings.separation_weight = 0.65f;
            }
            return steer(body, target, obstacles, settings);
        };
        // Sleeping agents participate as static collision obstacles. Restore complete
        // state after physics so gravity, impulses and timers cannot advance them.
        std::vector<std::pair<std::size_t, Body>> sleeping;
        for (auto &agent : agents_) {
            auto &body = world.body(agent.body_id());
            if (!agent.active()) {
                sleeping.emplace_back(agent.body_id(), body);
                body.motion = Motion::fixed;
                body.velocity = {};
                body.angular_velocity = 0;
                continue;
            }
            body.acceleration = acceleration(agent.body_id(), agent.advance_navigation(dt, body, navigation));
            body.angular_velocity = 0;
            if (length(body.velocity) > 0.1f)
                body.angular_velocity = std::clamp(
                    std::remainder(std::atan2(body.velocity.y, body.velocity.x) - body.angle, 6.283185307f) /
                        0.2f,
                    -5.f, 5.f);
        }
        auto &p = world.body(player);
        const Vec2 direction = control.direction;
        if (length(direction) > 0 || control.turn_left || control.turn_right) {
            player_goal.reset();
            player_path.clear();
        }
        if (player_goal) {
            player_replan -= dt;
            if (player_replan <= 0) {
                player_path = navigation.path(p.position, *player_goal);
                player_replan = 2;
            }
            p.acceleration = acceleration(player, route_target(p.position, player_path, navigation));
            if (length(*player_goal - p.position) < 0.4f) {
                player_goal.reset();
                player_path.clear();
            }
        } else {
            p.acceleration = limited((normalized(direction) * 7 - p.velocity) / 0.15f, 25);
        }
        p.angular_velocity = (float(control.turn_left) - float(control.turn_right)) * 2;
        if (player_goal && length(p.velocity) > 0.1f)
            p.angular_velocity = std::clamp(
                std::remainder(std::atan2(p.velocity.y, p.velocity.x) - p.angle, 6.283185307f) / 0.2f, -5.f,
                5.f);
        world.step(dt);
        for (const auto &[id, body] : sleeping)
            world.body(id) = body;
    }

  private:
    std::vector<Agent> agents_;
    std::set<Terrain::Coordinate> populated_;

    /// Reject overlapping spawn candidates, including the reserved player/passive area.
    bool spawn(Vec2 position, const Terrain::Chunks &chunks) {
        if (std::abs(position.x) < 30 && std::abs(position.y) < 20)
            return false;
        Body b;
        b.position = position;
        b.motion = Motion::controlled;
        b.shape = agents_.size() % 3 == 0 ? Shape::box : Shape::circle;
        b.motion = agents_.size() % 4 == 0 ? Motion::dynamic : Motion::controlled;
        b.half_extents = {0.4f, 0.25f};
        b.radius = 0.32f;
        b.friction = 0;
        for (const auto &other : world.bodies())
            if (length(other.position - position) < 1.5f)
                return false;
        for (const auto &[coordinate, obstacles] : chunks)
            for (const auto &obstacle : obstacles)
                if (collide(b, obstacle))
                    return false;
        agents_.emplace_back(world.add(b), position, terrain.seed());
        return true;
    }
};
} // namespace sandbox
