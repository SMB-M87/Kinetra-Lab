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
#include <utility>
#include <vector>

namespace demo {

/// Application-owned population, routes and terrain. Eviction remaps body indices.
///
/// Distances are world units, times simulated seconds. The world holds the moving bodies
/// first (body ids below moving_body_count(): agents, the player and six passive props),
/// then the loaded terrain; every agent's body id is a moving body other than the player.
/// stream() keeps the terrain, the population and the navigation around the player; update()
/// runs one fixed tick. A value type: a copy is an independent scene.
class Scene {
  public:
    /// Largest population; streaming spawns no agent beyond it.
    static constexpr std::size_t maximum_agents = 1200;

    /// `count` agents (0 to 300) at seeded points around the player's start at (0, -18),
    /// the player and the props, with terrain from `seed`. Terrain loads with the first
    /// stream(). Throws std::invalid_argument for another count, and std::runtime_error
    /// when an agent finds no free place in 1000 attempts.
    explicit Scene(int count, std::uint32_t seed);

    /// The physics world: the moving bodies first (agents, player, props), then the loaded
    /// terrain. Streaming may renumber bodies.
    const kinetra::World &world() const { return world_; }
    /// The terrain chunks that physics uses, around the player.
    const Terrain &terrain() const { return terrain_; }
    /// Terrain sampled around a ghost camera, for drawing only.
    const Terrain &preview() const { return preview_; }
    /// The route planner over the area around the player's active view.
    const kinetra::Navigation &navigation() const { return navigation_; }
    /// Read-only population view. Streaming may invalidate references and remap body IDs.
    std::span<const Agent> agents() const { return agents_; }
    /// Number of moving bodies, which occupy world body ids 0 up to this count.
    std::size_t moving_body_count() const { return moving_body_count_; }
    /// World body id of the player.
    std::size_t player() const { return player_; }
    /// Waypoints of the player's route; empty while the player has no goal.
    std::span<const kinetra::Vec2> player_path() const { return player_path_; }
    /// Where the player walks to, if anywhere.
    std::optional<kinetra::Vec2> player_goal() const { return player_goal_; }
    /// Home of the selected agent, which identifies it across renumbering.
    std::optional<kinetra::Vec2> selected_agent() const { return selected_agent_; }
    /// Whether the camera follows the player (false for a ghost camera).
    bool following() const { return following_; }
    /// The world area the camera saw at the last stream().
    kinetra::Aabb view() const { return view_; }
    /// Whether agents use reciprocal avoidance (the default) rather than legacy steering.
    bool reciprocal_avoidance() const { return reciprocal_avoidance_; }
    /// Awake agents whose avoidance could not meet every constraint in the last tick.
    std::size_t avoidance_limited() const { return avoidance_limited_; }
    /// Motion and wander values that update() reads on every tick; a debug menu may change
    /// them between ticks. update() throws for values that validate() rejects.
    Tuning &tuning() { return tuning_; }
    /// Read-only access to the motion and wander values.
    const Tuning &tuning() const { return tuning_; }

    /// Makes the camera follow the player, or leaves it free for a ghost camera.
    void set_following(bool following) { following_ = following; }
    /// Chooses reciprocal avoidance or legacy steering for the next ticks.
    void set_reciprocal_avoidance(bool reciprocal) { reciprocal_avoidance_ = reciprocal; }
    /// Sets the world's gravity in world units per second squared; streaming keeps it.
    void set_gravity(kinetra::Vec2 gravity) { world_.gravity = gravity; }
    /// Forgets the selected agent.
    void clear_selection() { selected_agent_.reset(); }

    /// A moving body, to place it: the player, a prop or an agent's body. Ids at or above
    /// moving_body_count() name terrain and throw std::out_of_range. The reference is valid
    /// until the next stream() or arrange().
    kinetra::Body &moving_body(std::size_t id);

    /// Replaces the world with a hand-built layout, for tests and tools that need exact
    /// geometry: `movers` become body ids 0 to `movers.size()`, `player` is one of them, and
    /// `obstacles` follow as the loaded terrain, which the navigation over
    /// `navigation_area` plans around and agents without a route avoid. Gravity, agents and
    /// tuning stay; the player's goal and route are dropped and agent routes replanned.
    ///
    /// Throws std::invalid_argument, before changing anything, when `player` is not an
    /// index into `movers`, or an agent's body id is not an index into `movers` or is the
    /// player. Throws what Navigation::rebuild throws for an invalid `navigation_area`. A
    /// later stream() that loads terrain replaces the layout with streamed terrain.
    void arrange(std::vector<kinetra::Body> movers, std::size_t player, std::vector<kinetra::Body> obstacles,
                 kinetra::Aabb navigation_area);

    /// Retain player-centered terrain and populate newly active chunks once. The population
    /// is capped; distant agents are evicted and revisits can repopulate chunks.
    ///
    /// The areas scale with the camera's viewport, not with its zoom or position: physics
    /// terrain and the agents it keeps lie within 1.25 times the view at the widest debug
    /// zoom (5 pixels per unit), agents wake within 1.75 times the view at the widest
    /// normal zoom (17), and navigation covers that waking area plus 24 units. The view
    /// itself only decides what the ghost preview samples. Throws what
    /// Camera2D::validate() or Terrain::update() throws.
    void stream(const kinetra::Camera2D &camera);

    /// Teleport only into free loaded space and discard previous navigation/momentum.
    /// Returns false, changing nothing, when `destination` is not finite, lies outside the
    /// view, or the player would overlap terrain or another body there.
    bool teleport(kinetra::Vec2 destination);

    /// Plans a route for the player to `destination`, or to the nearest reachable point
    /// when it is occupied. Returns false, leaving the player without a goal, when no route
    /// exists. Player input cancels the walk.
    bool walk_to(kinetra::Vec2 destination);

    /// A click at `destination`: on an awake agent (within one unit) it selects it, or
    /// deselects it when it was selected; elsewhere it commands the selected agent there.
    /// Returns false when the click did neither, so the caller can walk the player.
    bool select_or_target(kinetra::Vec2 destination);

    /// Commands every awake agent to `destination` and clears the selection.
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

    kinetra::World world_;
    Terrain terrain_;
    Terrain preview_;
    kinetra::Navigation navigation_;
    std::size_t moving_body_count_{};
    std::size_t player_{};
    Tuning tuning_;
    bool reciprocal_avoidance_{true};
    std::size_t avoidance_limited_{};
    std::vector<kinetra::Vec2> player_path_;
    std::optional<kinetra::Vec2> player_goal_;
    std::optional<kinetra::Vec2> selected_agent_;
    /// Seconds until the player's route is planned again.
    float player_replan_{};
    bool following_{true};
    kinetra::Aabb view_{};
    std::vector<Agent> agents_;
    std::set<Terrain::Coordinate> populated_;
    /// The terrain bodies that stream() last put into the world, and an index of their
    /// bounds for the agents that avoid terrain without a route. Index i is loaded_terrain_[i].
    std::vector<kinetra::Body> loaded_terrain_;
    kinetra::SpatialGrid terrain_index_{4};

    /// What every mover's steering in one tick shares; defined in scene.cpp.
    struct Tick;

    /// Places `count` agents at seeded free points around the player's start.
    void populate_initial(int count, std::uint32_t seed);
    /// Adds the player and the six passive props after the initial agents.
    void add_player_and_props();
    /// Removes the movers outside `retained`, except the player, and renumbers the rest.
    /// Returns whether any was removed; then the world holds only the movers.
    bool evict_distant(kinetra::Aabb retained);
    /// Replaces the world's bodies with `movers`, dropping the loaded terrain.
    void keep_only(std::span<const kinetra::Body> movers);
    /// Spawns agents once in each chunk that overlaps `active_view` for the first time.
    /// `terrain_dropped` says whether the world already holds only `movers`. Returns
    /// whether a chunk was populated; then the world holds only the movers.
    bool populate_chunks(kinetra::Aabb active_view, std::span<const kinetra::Body> movers,
                         bool terrain_dropped);
    /// Appends the terrain within `retained` to a world that holds only movers, rebuilds
    /// the terrain index and the navigation around `active_view`, and invalidates routes.
    void load_terrain(kinetra::Aabb retained, kinetra::Aabb active_view);
    /// The steering settings and the snapshot of the moving bodies for one tick.
    Tick prepare_tick() const;
    /// The acceleration that steers mover `id` toward `target` among the snapshot's
    /// neighbors, and around nearby terrain when `terrain_avoidance` says so.
    kinetra::Vec2 local_acceleration(std::size_t id, kinetra::Vec2 target, AvoidTerrain terrain_avoidance,
                                     float dt, Tick &tick);
    /// Sets the acceleration of every awake agent. Sleeping agents become fixed for the
    /// tick; returns their bodies as they were, to restore after physics.
    std::vector<std::pair<std::size_t, kinetra::Body>> steer_agents(float dt, Tick &tick);
    /// Sets the player's acceleration and turn from `control`, or from its route.
    void steer_player(float dt, const PlayerControl &control, Tick &tick);
    /// Reject overlapping spawn candidates, including the reserved player/passive area.
    bool spawn(kinetra::Vec2 position, const Terrain::Chunks &chunks);
};
} // namespace demo
