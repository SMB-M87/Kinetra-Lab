#pragma once

#include "demo/input/player_control.h"
#include "demo/world/agent.h"
#include "demo/world/terrain.h"
#include "demo/world/tuning.h"
#include <kinetra/simulation/navigation.h>
#include <kinetra/simulation/physics.h>
#include <kinetra/simulation/spatial_grid.h>
#include <kinetra/view/camera.h>

#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace demo {

/// Application-owned population, routes and terrain. Eviction remaps body indices.
class Scene {
  public:
    kinetra::World world;
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
    std::vector<kinetra::Vec2> player_path;
    std::optional<kinetra::Vec2> player_goal;
    std::optional<kinetra::Vec2> selected_agent;
    float player_replan{};
    bool following{true};
    kinetra::Aabb view{};
    static constexpr std::size_t maximum_agents = 1200;

    explicit Scene(int count, std::uint32_t seed);

    /// Read-only population view. Streaming may invalidate references and remap body IDs.
    std::span<const Agent> agents() const { return agents_; }

    /// Retain player-centered terrain and populate newly active chunks once. The population
    /// is capped; distant agents are evicted and revisits can repopulate chunks.
    void stream(const kinetra::Camera2D &camera);

    /// Teleport only into free loaded space and discard previous navigation/momentum.
    bool teleport(kinetra::Vec2 destination);

    bool walk_to(kinetra::Vec2 destination);

    bool select_or_target(kinetra::Vec2 destination);

    void target_agents(kinetra::Vec2 destination);

    /// Follow the latest player pose, including while paused or after teleportation.
    void follow(kinetra::Camera2D &camera) const;

    /// Apply player intent and active-agent steering for one fixed tick; sleeping
    /// agents remain collision obstacles and regain their exact state after physics.
    ///
    /// Reads `tuning` and throws std::invalid_argument, before anything moves, when
    /// validate() rejects it.
    void update(float dt, const PlayerControl &control);

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
    std::vector<kinetra::Body> loaded_terrain_;
    kinetra::SpatialGrid terrain_index_{4};

    /// Reject overlapping spawn candidates, including the reserved player/passive area.
    bool spawn(kinetra::Vec2 position, const Terrain::Chunks &chunks);
};
} // namespace demo
