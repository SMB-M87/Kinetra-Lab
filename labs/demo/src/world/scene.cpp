#include "demo/world/scene.h"

#include <algorithm>
#include <cmath>
#include <kinetra/simulation/avoidance.h>
#include <kinetra/simulation/steering.h>
#include <random>
#include <stdexcept>
#include <utility>

namespace demo {
using namespace kinetra;
namespace {
/// Largest number of circles that stand in for one terrain box in reciprocal avoidance.
constexpr std::size_t max_box_circles = 6;

/// Appends to `circles` the circles that cover terrain body `obstacle`, for reciprocal
/// avoidance, which knows moving circles but no rotated boxes. A circle stands for itself.
/// A box becomes a row of equal circles along its long side: as many as the short side fits
/// into the long one, at most max_box_circles, each just large enough to cover its part of
/// the box. One bounding circle would also block the free space beside a long wall. Any
/// other shape gets its bounding circle.
void append_avoidance_circles(const Body &obstacle, std::vector<Circle> &circles) {
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
/// Turn rate in radians per second that turns `body` toward its direction of travel within
/// about 0.2 seconds, at most 5 radians per second either way.
float heading_turn_rate(const Body &body) {
    return std::clamp(
        std::remainder(std::atan2(body.velocity.y, body.velocity.x) - body.angle, 6.283185307f) / 0.2f, -5.f,
        5.f);
}
} // namespace

Scene::Scene(int count, std::uint32_t seed) : terrain_(seed), preview_(seed) {
    if (count < 0 || count > 300)
        throw std::invalid_argument("Initial agent count must be 0..300");
    populate_initial(count, seed);
    add_player_and_props();
    moving_body_count_ = world_.bodies().size();
    // Initial agents already populate these chunks; exploration adds agents elsewhere.
    for (int y = -2; count > 0 && y <= 0; ++y)
        for (int x = -2; x <= 1; ++x)
            populated_.emplace(x, y);
}

void Scene::populate_initial(int count, std::uint32_t seed) {
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
}

void Scene::add_player_and_props() {
    Body b;
    b.motion = Motion::controlled;
    b.shape = Shape::box;
    b.position = {0, -18};
    b.half_extents = {0.65f, 0.4f};
    b.friction = 0;
    player_ = world_.add(b);
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
        world_.add(ball);
    }
}

Body &Scene::moving_body(std::size_t id) {
    if (id >= moving_body_count_)
        throw std::out_of_range("Scene body id is not a moving body");
    return world_.body(id);
}

void Scene::arrange(std::vector<Body> movers, std::size_t player, std::vector<Body> obstacles,
                    Aabb navigation_area) {
    if (player >= movers.size())
        throw std::invalid_argument("Arranged player must be one of the movers");
    for (const auto &agent : agents_)
        if (agent.body_id() >= movers.size() || agent.body_id() == player)
            throw std::invalid_argument("Every agent needs an arranged mover other than the player");
    // Navigation validates before it replaces its snapshot, so a rejected area leaves the
    // scene as it was.
    navigation_.rebuild(navigation_area, obstacles);
    world_.clear();
    for (const auto &body : movers)
        world_.add(body);
    std::vector<Aabb> obstacle_bounds;
    for (const auto &body : obstacles) {
        world_.add(body);
        obstacle_bounds.push_back(bounds(body));
    }
    terrain_index_.rebuild(obstacle_bounds);
    loaded_terrain_ = std::move(obstacles);
    moving_body_count_ = movers.size();
    player_ = player;
    player_goal_.reset();
    player_path_.clear();
    player_replan_ = 0;
    for (auto &agent : agents_)
        agent.invalidate_route();
}

void Scene::stream(const Camera2D &camera) {
    camera.validate();
    const Vec2 half_view = camera.viewport / (2 * camera.pixels_per_unit);
    view_ = {camera.position - half_view, camera.position + half_view};
    if (!following_)
        preview_.update(view_, 0);
    // Lifecycle distances are player-centered and independent of Ghost and current zoom.
    const auto center = world_.body(player_).position;
    const Vec2 active_half = camera.viewport * (1.75f / (2 * 17.f));
    const Aabb active_view{center - active_half, center + active_half};
    const Vec2 retained_half = camera.viewport * (1.25f / (2 * 5.f));
    const Aabb retained{center - retained_half, center + retained_half};
    const bool removed = evict_distant(retained);
    // Remember only resident chunks, allowing bounded state and fresh populations on return.
    std::erase_if(populated_, [&](const auto &coordinate) {
        const Vec2 corner{float(coordinate.first) * 32.f, float(coordinate.second) * 32.f};
        return !retained.overlaps({corner, corner + Vec2{32, 32}});
    });
    const bool changed = terrain_.update(retained, 0) || removed;
    const std::vector<Body> movers(world_.bodies().begin(), world_.bodies().begin() + moving_body_count_);
    if (changed)
        keep_only(movers);
    const bool added = populate_chunks(active_view, movers, changed);
    if (changed || added)
        load_terrain(retained, active_view);
    for (auto &agent : agents_)
        agent.update_visibility(active_view, world_.body(agent.body_id()));
}

bool Scene::evict_distant(Aabb retained) {
    bool removed = false;
    std::vector<bool> discard(moving_body_count_);
    for (std::size_t id = 0; id < moving_body_count_; ++id)
        if (id != player_ && !retained.overlaps(bounds(world_.body(id)))) {
            discard[id] = true;
            removed = true;
        }
    if (!removed)
        return false;
    // Compact movers and remap every handle atomically before rebuilding terrain.
    std::vector<std::size_t> remap(moving_body_count_);
    std::vector<Body> retained_bodies;
    for (std::size_t i = 0; i < moving_body_count_; ++i)
        if (!discard[i]) {
            remap[i] = retained_bodies.size();
            retained_bodies.push_back(world_.body(i));
        }
    std::erase_if(agents_, [&](const Agent &a) { return discard[a.body_id()]; });
    for (auto &agent : agents_)
        agent.remap_body(remap[agent.body_id()]);
    player_ = remap[player_];
    keep_only(retained_bodies);
    moving_body_count_ = retained_bodies.size();
    return true;
}

void Scene::keep_only(std::span<const Body> movers) {
    world_.clear();
    for (const auto &body : movers)
        world_.add(body);
}

bool Scene::populate_chunks(Aabb active_view, std::span<const Body> movers, bool terrain_dropped) {
    bool added = false;
    for (const auto &[coordinate, obstacles] : terrain_.chunks()) {
        const Vec2 corner{float(coordinate.first) * Terrain::chunk_size,
                          float(coordinate.second) * Terrain::chunk_size};
        if (!active_view.overlaps({corner, corner + Vec2{32, 32}}) || agents_.size() >= maximum_agents ||
            !populated_.insert(coordinate).second)
            continue;
        std::seed_seq sequence{terrain_.seed(), std::uint32_t(coordinate.first),
                               std::uint32_t(coordinate.second), 12345u};
        std::mt19937 random(sequence);
        // Remove disposable obstacles before inserting movers to retain stable indices.
        if (!terrain_dropped && !added)
            keep_only(movers);
        added = true;
        for (int attempt = 0, made = 0; attempt < 160 && made < 12 && agents_.size() < maximum_agents;
             ++attempt) {
            const auto unit = [&]() { return float(random() >> 8) / 16777216.f; };
            if (spawn(corner + Vec2{2 + unit() * 28, 2 + unit() * 28}, terrain_.chunks()))
                ++made;
        }
    }
    return added;
}

void Scene::load_terrain(Aabb retained, Aabb active_view) {
    moving_body_count_ = world_.bodies().size();
    loaded_terrain_.clear();
    std::vector<Aabb> terrain_bounds;
    for (const auto &[coordinate, bodies] : terrain_.chunks())
        for (const auto &body : bodies) {
            if (!retained.overlaps(bounds(body)))
                continue;
            world_.add(body);
            loaded_terrain_.push_back(body);
            terrain_bounds.push_back(bounds(body));
        }
    // Terrain changes only here, so its index is rebuilt here and not every tick.
    terrain_index_.rebuild(terrain_bounds);
    // Plan only around simulated actors, rather than allocating navigation
    // for the much larger debug residency area. Margin covers wander goals.
    const Vec2 route_margin{24, 24};
    navigation_.rebuild({active_view.min - route_margin, active_view.max + route_margin}, loaded_terrain_);
    for (auto &agent : agents_)
        agent.invalidate_route();
    player_replan_ = 0;
}

bool Scene::teleport(Vec2 destination) {
    if (!finite(destination) || !view_.overlaps({destination, destination}))
        return false;
    Body candidate = world_.body(player_);
    candidate.position = destination;
    if (terrain_.blocked(candidate))
        return false;
    for (std::size_t i = 0; i < world_.bodies().size(); ++i)
        if (i != player_ && collide(candidate, world_.body(i)))
            return false;
    candidate.velocity = candidate.acceleration = {};
    candidate.angular_velocity = 0;
    world_.body(player_) = candidate;
    player_goal_.reset();
    player_path_.clear();
    return true;
}

bool Scene::walk_to(Vec2 destination) {
    player_path_ = navigation_.path(world_.body(player_).position, destination);
    player_goal_ = player_path_.empty() ? std::nullopt : std::optional<Vec2>(player_path_.back());
    player_replan_ = 2;
    return player_goal_.has_value();
}

bool Scene::select_or_target(Vec2 destination) {
    for (auto &agent : agents_)
        if (agent.active() && length(world_.body(agent.body_id()).position - destination) < 1.f) {
            if (selected_agent_ && length(*selected_agent_ - agent.home()) < 0.001f)
                selected_agent_.reset();
            else
                selected_agent_ = agent.home();
            return true;
        }
    if (selected_agent_) {
        for (auto &agent : agents_)
            if (length(agent.home() - *selected_agent_) < 0.001f) {
                agent.retarget(destination);
                return true;
            }
        selected_agent_.reset();
    }
    return false;
}

void Scene::target_agents(Vec2 destination) {
    selected_agent_.reset();
    for (auto &agent : agents_)
        agent.retarget(destination);
}

void Scene::follow(Camera2D &camera) const {
    if (following_)
        camera.position = world_.body(player_).position;
}

/// What every mover's steering in one tick shares: the settings and the snapshot of the
/// moving bodies that all of them read, so the order in which agents steer cannot matter.
struct Scene::Tick {
    Steering steering;
    AvoidanceSettings avoidance;
    /// The moving bodies at the start of the tick; sleeping agents as fixed, still bodies.
    std::vector<Body> snapshot;
    /// Per moving body: whether it shares the avoidance effort (an awake agent).
    std::vector<bool> cooperative;
    /// The snapshot's bounds, for neighbor queries.
    SpatialGrid neighbors{4};
    /// Largest bounding radius and speed in the snapshot (the speed at least the top
    /// speed), which bound how far away a neighbor can still matter.
    float largest_radius{};
    float largest_speed{};
    /// Scratch space for the circles of one terrain body.
    std::vector<Circle> terrain_circles;
};

void Scene::update(float dt, const PlayerControl &control) {
    validate(tuning_);
    avoidance_limited_ = 0;
    Tick tick = prepare_tick();
    // Sleeping agents participate as static collision obstacles. Restore complete
    // state after physics so gravity, impulses and timers cannot advance them.
    const auto sleeping = steer_agents(dt, tick);
    steer_player(dt, control, tick);
    world_.step(dt);
    for (const auto &[id, body] : sleeping)
        world_.body(id) = body;
}

Scene::Tick Scene::prepare_tick() const {
    const MotionTuning &motion = tuning_.motion;
    Tick tick;
    Steering &steering = tick.steering;
    steering.max_speed = motion.max_speed;
    steering.max_acceleration = motion.max_acceleration;
    steering.response_time = motion.response_time;
    steering.lookahead = motion.lookahead;
    steering.slow_radius = slow_radius;
    steering.stop_radius = stop_radius;
    steering.clearance = motion.clearance;
    tick.snapshot.assign(world_.bodies().begin(), world_.bodies().begin() + moving_body_count_);
    tick.cooperative.assign(moving_body_count_, false);
    for (const auto &agent : agents_) {
        tick.cooperative[agent.body_id()] = agent.active();
        if (!agent.active()) {
            auto &body = tick.snapshot[agent.body_id()];
            body.motion = Motion::fixed;
            body.velocity = {};
            body.angular_velocity = 0;
        }
    }
    std::vector<Aabb> boxes;
    tick.largest_radius = 0;
    tick.largest_speed = steering.max_speed;
    for (std::size_t id = 0; id < moving_body_count_; ++id) {
        boxes.push_back(bounds(tick.snapshot[id]));
        tick.largest_radius = std::max(tick.largest_radius, bounding_radius(tick.snapshot[id]));
        tick.largest_speed = std::max(tick.largest_speed, length(tick.snapshot[id].velocity));
    }
    tick.neighbors.rebuild(boxes);
    AvoidanceSettings &avoidance = tick.avoidance;
    avoidance.max_speed = steering.max_speed;
    avoidance.max_acceleration = steering.max_acceleration;
    avoidance.clearance = steering.clearance;
    avoidance.horizon = motion.horizon;
    avoidance.passing_bias = motion.passing_bias;
    return tick;
}

Vec2 Scene::local_acceleration(std::size_t id, Vec2 target, AvoidTerrain terrain_avoidance, float dt,
                               Tick &tick) {
    const Steering &steering = tick.steering;
    const AvoidanceSettings &avoidance = tick.avoidance;
    const auto &body = tick.snapshot[id];
    std::vector<Body> obstacles;
    std::vector<AvoidanceAgent> agents;
    const bool reciprocal = reciprocal_avoidance_ && id != player_;
    const float range = reciprocal ? bounding_radius(body) + tick.largest_radius +
                                         (avoidance.max_speed + tick.largest_speed) * avoidance.horizon
                                   : legacy_neighbor_range;
    const Vec2 reach{range, range};
    for (auto other : tick.neighbors.query({body.position - reach, body.position + reach}))
        // Terrain clearance is handled by the swept route. Local avoidance
        // handles movers without fighting planned static-obstacle corners. Only
        // an agent without a route adds terrain below.
        if (other != id && other < moving_body_count_) {
            if (reciprocal) {
                const auto &neighbor = tick.snapshot[other];
                agents.push_back({other, neighbor.position, neighbor.velocity, bounding_radius(neighbor),
                                  tick.cooperative[other]});
            } else {
                obstacles.push_back(tick.snapshot[other]);
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
            auto &terrain_circles = tick.terrain_circles;
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
        const auto preferred = body.velocity + steer(body, target, {}, steering) * steering.response_time;
        const auto result = avoid_collisions({id, body.position, body.velocity, bounding_radius(body)},
                                             preferred, agents, dt, avoidance);
        if (!result.feasible)
            ++avoidance_limited_;
        return result.acceleration;
    }
    auto settings = steering;
    if (id != player_) {
        settings.separation_radius = tuning_.motion.separation_radius;
        settings.separation_weight = tuning_.motion.separation_weight;
    }
    return steer(body, target, obstacles, settings);
}

std::vector<std::pair<std::size_t, Body>> Scene::steer_agents(float dt, Tick &tick) {
    std::vector<std::pair<std::size_t, Body>> sleeping;
    for (auto &agent : agents_) {
        auto &body = world_.body(agent.body_id());
        if (!agent.active()) {
            sleeping.emplace_back(agent.body_id(), body);
            body.motion = Motion::fixed;
            body.velocity = {};
            body.angular_velocity = 0;
            continue;
        }
        const Vec2 target = agent.advance_navigation(dt, body, navigation_, tuning_);
        // A route keeps its own clearance from terrain; an agent without one relies on
        // local avoidance to stay clear of it.
        body.acceleration =
            local_acceleration(agent.body_id(), target,
                               agent.steers_without_route() ? AvoidTerrain::yes : AvoidTerrain::no, dt, tick);
        body.angular_velocity = 0;
        if (length(body.velocity) > 0.1f)
            body.angular_velocity = heading_turn_rate(body);
    }
    return sleeping;
}

void Scene::steer_player(float dt, const PlayerControl &control, Tick &tick) {
    auto &p = world_.body(player_);
    const Vec2 direction = control.direction;
    if (length(direction) > 0 || control.turn_left || control.turn_right) {
        player_goal_.reset();
        player_path_.clear();
    }
    if (player_goal_) {
        player_replan_ -= dt;
        if (player_replan_ <= 0) {
            player_path_ = navigation_.path(p.position, *player_goal_);
            player_replan_ = 2;
        }
        p.acceleration = local_acceleration(player_, route_target(p.position, player_path_, navigation_),
                                            AvoidTerrain::no, dt, tick);
        if (length(*player_goal_ - p.position) < 0.4f) {
            player_goal_.reset();
            player_path_.clear();
        }
    } else {
        p.acceleration = limited((normalized(direction) * 7 - p.velocity) / 0.15f, 25);
    }
    p.angular_velocity = (float(control.turn_left) - float(control.turn_right)) * 2;
    if (player_goal_ && length(p.velocity) > 0.1f)
        p.angular_velocity = heading_turn_rate(p);
}

bool Scene::spawn(Vec2 position, const Terrain::Chunks &chunks) {
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
    for (const auto &other : world_.bodies())
        if (length(other.position - position) < 1.5f)
            return false;
    for (const auto &[coordinate, obstacles] : chunks)
        for (const auto &obstacle : obstacles)
            if (collide(b, obstacle))
                return false;
    agents_.emplace_back(world_.add(b), position, terrain_.seed());
    return true;
}
} // namespace demo
