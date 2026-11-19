#pragma once

#include "agent.h"
#include "player_control.h"
#include "terrain.h"
#include "tuning.h"
#include <algorithm>
#include <cmath>
#include <kinetra/avoidance.h>
#include <kinetra/camera.h>
#include <kinetra/steering.h>
#include <optional>
#include <set>

namespace sandbox {
using namespace kinetra;

/// Largest number of circles that stand in for one terrain box in reciprocal avoidance.
inline constexpr std::size_t max_box_circles = 6;

/// Appends to `circles` the circles that cover terrain body `obstacle`, for reciprocal
/// avoidance, which knows moving circles but no rotated boxes. A circle stands for itself.
/// A box becomes a row of equal circles along its long side: as many as the short side fits
/// into the long one, at most max_box_circles, each just large enough to cover its part of
/// the box. One bounding circle would also block the free space beside a long wall. Any
/// other shape gets its bounding circle.
inline void append_avoidance_circles(const Body &obstacle, std::vector<Circle> &circles) {
    if (obstacle.shape == Shape::circle) {
        circles.push_back({obstacle.position, obstacle.radius});
        return;
    }
    if (obstacle.shape != Shape::box) {
        circles.push_back({obstacle.position, bounding_radius(obstacle)});
        return;
    }
    const Vec2 half = obstacle.half_extents;
    const float long_half = std::max(half.x, half.y), short_half = std::min(half.x, half.y);
    const Vec2 long_axis = rotated(half.x >= half.y ? Vec2{1, 0} : Vec2{0, 1}, obstacle.angle);
    // The float clamp also keeps a degenerate ratio (infinite for a zero short side) in range.
    const auto count = static_cast<std::size_t>(
        std::clamp(std::ceil(long_half / short_half), 1.f, static_cast<float>(max_box_circles)));
    // Each circle covers a part of the box that is `part_half` long and `short_half` wide,
    // both measured from the circle's center, so it reaches the corners of that part.
    const float part_half = long_half / static_cast<float>(count);
    const float radius = length(Vec2{short_half, part_half});
    for (std::size_t i = 0; i < count; ++i)
        circles.push_back(
            {obstacle.position + long_axis * (part_half * static_cast<float>(2 * i + 1) - long_half),
             radius});
}

/// Application-owned population, routes and terrain. Eviction remaps body indices.
class Sandbox {
  public:
    World world;
    Terrain terrain;
    Terrain preview;
    kinetra::Navigation navigation;
    std::size_t moving_body_count{};
    std::size_t player{};
    /// Motion and wander values that update() reads on every tick; a debug menu may change
    /// them between ticks. update() throws for values that validate() rejects.
    Tuning tuning;
    bool reciprocal_avoidance{true};
    std::size_t avoidance_limited{};
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
                const Vec2 position{-36 + unit() * 72, -42 + unit() * 48};
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
            if (i >= 2)
                ball = regular_polygon(i == 2 ? 3 : i == 3 ? 5 : 6, 0.5f);
            else
                ball.shape = i % 2 ? Shape::circle : Shape::box;
            ball.position = {-22.f + float(i) * 8, 16};
            ball.velocity = {i % 2 ? -4.f : 4.f, -1};
            ball.angle = 0.35f * float(i);
            ball.angular_velocity = 0.7f;
            ball.restitution = 0.9f;
            world.add(ball);
        }
        moving_body_count = world.bodies().size();
        // Initial agents already populate these chunks; exploration adds agents elsewhere.
        for (int y = -2; count > 0 && y <= 0; ++y)
            for (int x = -2; x <= 1; ++x)
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
            const Vec2 corner{float(coordinate.first) * 32.f, float(coordinate.second) * 32.f};
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
            const Vec2 corner{float(coordinate.first) * Terrain::chunk_size,
                              float(coordinate.second) * Terrain::chunk_size};
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
            for (int attempt = 0, made = 0; attempt < 160 && made < 12 && agents_.size() < maximum_agents;
                 ++attempt) {
                const auto unit = [&]() { return float(random() >> 8) / 16777216.f; };
                if (spawn(corner + Vec2{2 + unit() * 28, 2 + unit() * 28}, terrain.chunks()))
                    ++made;
            }
        }
        if (changed || added) {
            moving_body_count = world.bodies().size();
            loaded_terrain_.clear();
            std::vector<Aabb> terrain_bounds;
            for (const auto &[coordinate, bodies] : terrain.chunks())
                for (const auto &body : bodies) {
                    if (!retained.overlaps(bounds(body)))
                        continue;
                    world.add(body);
                    loaded_terrain_.push_back(body);
                    terrain_bounds.push_back(bounds(body));
                }
            // Terrain changes only here, so its index is rebuilt here and not every tick.
            terrain_index_.rebuild(terrain_bounds);
            // Plan only around simulated actors, rather than allocating navigation
            // for the much larger debug residency area. Margin covers wander goals.
            const Vec2 route_margin{24, 24};
            navigation.rebuild({active_view.min - route_margin, active_view.max + route_margin},
                               loaded_terrain_);
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
    ///
    /// Reads `tuning` and throws std::invalid_argument, before anything moves, when
    /// validate() rejects it.
    void update(float dt, const PlayerControl &control) {
        validate(tuning);
        avoidance_limited = 0;
        const MotionTuning &motion = tuning.motion;
        Steering steering;
        steering.max_speed = motion.max_speed;
        steering.max_acceleration = motion.max_acceleration;
        steering.response_time = motion.response_time;
        steering.lookahead = motion.lookahead;
        steering.slow_radius = slow_radius;
        steering.stop_radius = stop_radius;
        steering.clearance = motion.clearance;
        std::vector<Body> snapshot(world.bodies().begin(), world.bodies().begin() + moving_body_count);
        std::vector<bool> cooperative(moving_body_count, false);
        for (const auto &agent : agents_) {
            cooperative[agent.body_id()] = agent.active();
            if (!agent.active()) {
                auto &body = snapshot[agent.body_id()];
                body.motion = Motion::fixed;
                body.velocity = {};
                body.angular_velocity = 0;
            }
        }
        std::vector<Aabb> boxes;
        float largest_radius = 0;
        float largest_speed = steering.max_speed;
        for (std::size_t id = 0; id < moving_body_count; ++id) {
            boxes.push_back(bounds(snapshot[id]));
            largest_radius = std::max(largest_radius, bounding_radius(snapshot[id]));
            largest_speed = std::max(largest_speed, length(snapshot[id].velocity));
        }
        SpatialGrid neighbors(4);
        neighbors.rebuild(boxes);
        AvoidanceSettings avoidance;
        avoidance.max_speed = steering.max_speed;
        avoidance.max_acceleration = steering.max_acceleration;
        avoidance.clearance = steering.clearance;
        avoidance.horizon = motion.horizon;
        avoidance.passing_bias = motion.passing_bias;
        // Scratch space for the circles of one terrain body.
        std::vector<Circle> terrain_circles;
        const auto acceleration = [&](std::size_t id, Vec2 target, AvoidTerrain terrain_avoidance) {
            const auto &body = snapshot[id];
            std::vector<Body> obstacles;
            std::vector<AvoidanceAgent> agents;
            const bool reciprocal = reciprocal_avoidance && id != player;
            const float range = reciprocal ? bounding_radius(body) + largest_radius +
                                                 (avoidance.max_speed + largest_speed) * avoidance.horizon
                                           : legacy_neighbor_range;
            const Vec2 reach{range, range};
            for (auto other : neighbors.query({body.position - reach, body.position + reach}))
                // Terrain clearance is handled by the swept route. Local avoidance
                // handles movers without fighting planned static-obstacle corners. Only
                // an agent without a route adds terrain below.
                if (other != id && other < moving_body_count) {
                    if (reciprocal) {
                        const auto &neighbor = snapshot[other];
                        agents.push_back({other, neighbor.position, neighbor.velocity,
                                          bounding_radius(neighbor), cooperative[other]});
                    } else {
                        obstacles.push_back(snapshot[other]);
                    }
                }
            if (terrain_avoidance == AvoidTerrain::yes) {
                // Terrain stands still, so only the body's own travel within the time that
                // its avoidance model looks ahead decides which terrain can matter.
                const float travel = std::max(steering.max_speed, length(body.velocity)) *
                                     (reciprocal ? avoidance.horizon : steering.lookahead);
                const float terrain_range = bounding_radius(body) + steering.clearance + travel;
                const Vec2 terrain_reach{terrain_range, terrain_range};
                for (auto index :
                     terrain_index_.query({body.position - terrain_reach, body.position + terrain_reach})) {
                    if (!reciprocal) {
                        obstacles.push_back(loaded_terrain_[index]);
                        continue;
                    }
                    terrain_circles.clear();
                    append_avoidance_circles(loaded_terrain_[index], terrain_circles);
                    const std::uint64_t first_id =
                        terrain_avoidance_ids + static_cast<std::uint64_t>(index) * max_box_circles;
                    for (std::size_t part = 0; part < terrain_circles.size(); ++part)
                        // Not cooperative: terrain never yields, the agent avoids it alone.
                        agents.push_back({first_id + part, terrain_circles[part].center, Vec2{},
                                          terrain_circles[part].radius, false});
                }
            }
            if (reciprocal) {
                const auto preferred =
                    body.velocity + steer(body, target, {}, steering) * steering.response_time;
                const auto result =
                    avoid_collisions({id, body.position, body.velocity, bounding_radius(body)}, preferred,
                                     agents, dt, avoidance);
                if (!result.feasible)
                    ++avoidance_limited;
                return result.acceleration;
            }
            auto settings = steering;
            if (id != player) {
                settings.separation_radius = motion.separation_radius;
                settings.separation_weight = motion.separation_weight;
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
            const Vec2 target = agent.advance_navigation(dt, body, navigation, tuning);
            // A route keeps its own clearance from terrain; an agent without one relies on
            // local avoidance to stay clear of it.
            body.acceleration = acceleration(
                agent.body_id(), target, agent.steers_without_route() ? AvoidTerrain::yes : AvoidTerrain::no);
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
            p.acceleration =
                acceleration(player, route_target(p.position, player_path, navigation), AvoidTerrain::no);
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
    /// Whether local avoidance of one mover also looks at the terrain near it.
    enum class AvoidTerrain { no, yes };
    /// Distance in world units within which legacy steering looks for other movers.
    static constexpr float legacy_neighbor_range = 6;
    /// First avoidance id of terrain circles. Movers use their body index as id, and the
    /// world never holds this many bodies, so the two kinds cannot collide.
    static constexpr std::uint64_t terrain_avoidance_ids = std::uint64_t{1} << 32;

    std::vector<Agent> agents_;
    std::set<Terrain::Coordinate> populated_;
    /// The terrain bodies that stream() last put into the world, and an index of their
    /// bounds for the agents that avoid terrain without a route. Index i is loaded_terrain_[i].
    std::vector<Body> loaded_terrain_;
    SpatialGrid terrain_index_{4};

    /// Reject overlapping spawn candidates, including the reserved player/passive area.
    bool spawn(Vec2 position, const Terrain::Chunks &chunks) {
        if (distance(position, {0, -18}) < 3 ||
            (position.x > -24 && position.x < 20 && std::abs(position.y - 16) < 2))
            return false;
        Body b;
        const auto variant = agents_.size() % 5;
        if (variant >= 2)
            b = regular_polygon(variant == 2 ? 3 : variant == 3 ? 5 : 6, 0.32f);
        b.position = position;
        b.motion = Motion::controlled;
        if (variant < 2)
            b.shape = variant == 0 ? Shape::box : Shape::circle;
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
