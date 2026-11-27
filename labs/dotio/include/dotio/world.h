#pragma once

#include "dotio/ai/tracker.h"
#include "dotio/difficulty.h"
#include "dotio/entities/arrow.h"
#include "dotio/entities/bolt.h"
#include "dotio/entities/enemy.h"
#include "dotio/entities/player.h"
#include "dotio/navigation/grid.h"

#include <kinetra/simulation/passing_coordinator.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <span>
#include <unordered_set>
#include <vector>

namespace dotio {
/// Own the streamed-world game population, projectiles and grid.
/// Addresses stay stable because enemies and their state objects reference this world.
class World {
    float tick_rate_{1.0f / 20.0f};
    dotio::Player player_;
    dotio::Player ghost_;
    TrackerData<Enemy, 666> enemies_;
    TrackerData<Arrow, 666> arrows_;
    TrackerData<Bolt, 20> bolts_;
    dotio::Grid grid_;
    std::array<kinetra::Vec2, 666> previous_enemy_positions_{};
    bool enemies_moved_{};
    float spawn_credit_{};
    double survival_seconds_{};
    std::uint64_t resets_{};
    Difficulty difficulty_{Difficulty::medium};
    /// The balance in use: the preset of difficulty_ until settings() changes it.
    DifficultySettings settings_;
    EnemyTuning enemy_tuning_{};
    std::uint32_t seed_{};
    std::mt19937 population_random_{0};
    /// Wander headings and their drift; its own stream, so spawning does not shift it.
    std::mt19937 wander_random_{0};
    /// Enemy fear and path-wander targets; its own stream, so spawning does not shift it.
    std::mt19937 behavior_random_{0};
    /// Spawn points of the player and the ghost, and the dormant pool's first positions.
    std::mt19937 placement_random_{0};
    int offset_row_{}, offset_column_{};
    std::array<int, 4> resident_cells_{-1, -1, -1, -1};
    struct Prop {
        int row, column; // Stable absolute spawn district, independent of body displacement.
        kinetra::Body body;
    };
    std::vector<Prop> props_;
    bool passing_{};
    kinetra::PassingCoordinator passing_coordinator_;
    /// Per pool slot, the Enemy::generation() of its occupant at the last passing step.
    std::array<std::uint64_t, 666> passing_generations_{};
    std::vector<kinetra::PassingEvent> passing_events_;
    void populate_objects();
    /// One passing step before the enemies move: sets every enemy's passing_target, and
    /// replans the route of every enemy whose plan ended.
    void coordinate_passing();
    /// Ends every passing plan and clears the enemies' passing targets; `replan` plans the
    /// routes of the participants again.
    void end_passing(bool replan);
    std::optional<std::pair<std::size_t, float>> first_object_hit(kinetra::Segment segment,
                                                                  float radius) const;

  public:
    /// Throws std::invalid_argument for a difficulty without a preset.
    explicit World(Difficulty difficulty = Difficulty::medium)
        : difficulty_(difficulty), settings_(settings_for(difficulty)) {}
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    /// The enemies registered in each cell, for neighbor separation; each enemy keeps its
    /// cells in PhysicsObject::last_partition.
    std::unordered_map<dotio::math::GridEntry, std::unordered_set<dotio::PhysicsObject *>>
        enemy_partition_map;

    /// Starts a new world for `seed` at `difficulty`: terrain, player, a dormant enemy pool,
    /// the preset of the difficulty as settings() and a default enemy_tuning(). The seed also
    /// seeds every random stream of the world, so the same seed and the same commands give the
    /// same run. Throws std::invalid_argument for a difficulty without a preset, before
    /// anything changes.
    void initialize(std::uint32_t seed, Difficulty difficulty = Difficulty::medium);
    /// The balance this world runs with. It starts as the preset of the difficulty and
    /// follows every change made through settings().
    const DifficultySettings &difficulty_settings() const { return settings_; }
    /// The balance of this world, for changing it while the world runs.
    ///
    /// The world reads the values where it uses them: movement, firing, pursuit and spawning
    /// follow at once, `enemy_hits` when an enemy next spawns. Values are not checked here;
    /// keep them within what validate(const DifficultySettings &) accepts. reset() keeps
    /// them, reset_tuning() and initialize() restore the preset.
    DifficultySettings &settings() { return settings_; }
    /// The wander mode and state forces of this world's enemies, read on every update.
    /// Values are not checked here; keep them within what validate(const EnemyTuning &)
    /// accepts. reset() keeps them, reset_tuning() and initialize() restore the defaults.
    EnemyTuning &enemy_tuning() { return enemy_tuning_; }
    const EnemyTuning &enemy_tuning() const { return enemy_tuning_; }
    /// Restores settings() to the preset of the difficulty and enemy_tuning() to its defaults.
    void reset_tuning();
    /// Generator behind steering wander: the headings enemies start with and their drift.
    /// Seeded from the world's seed in initialize(), so the same seed and the same calls
    /// give the same wander. Only the wander code draws from it.
    std::mt19937 &wander_random() { return wander_random_; }
    /// Generator behind enemy fear (on spawn, reset and reload) and the targets and jitter of
    /// path-mode wander. Seeded from the world's seed in initialize().
    std::mt19937 &behavior_random() { return behavior_random_; }

    /// Starts a new life after the player's death: respawns the player and retires every
    /// enemy and projectile. Difficulty, settings() and enemy_tuning() stay as they are.
    void reset();
    /// Number of reset() calls so far. A reset moves the player to a new spawn point, so
    /// drawing must not blend across one.
    std::uint64_t reset_count() const { return resets_; }
    /// Simulated seconds since the start or the last reset; pauses do not count.
    double survival_seconds() const { return survival_seconds_; }
    /// Half extents of the spawn rectangle around the player, in legacy pixels: 1.75 times
    /// `normal_half` at the start, growing to `active_half` after five simulated minutes.
    kinetra::Vec2 spawn_half(kinetra::Vec2 normal_half, kinetra::Vec2 active_half) const {
        const float progress = float(std::clamp(survival_seconds_ / 300.0, 0.0, 1.0));
        const auto initial = normal_half * 1.75f;
        return {std::min(active_half.x, initial.x + (active_half.x - initial.x) * progress),
                std::min(active_half.y, initial.y + (active_half.y - initial.y) * progress)};
    }
    /// Shifts the resident window by whole 16-cell steps when the player leaves cells 48 to
    /// 106 of it on an axis, regenerates the grid, and moves every object by the shift. Returns
    /// the shift in legacy pixels (zero when none), which the caller subtracts from its own
    /// positions. Routes and partitions are dropped.
    kinetra::Vec2 stream_around_player();
    /// Legacy position of the window's corner in the unbounded world: adding it turns a
    /// window position into one that streaming does not change.
    kinetra::Vec2 world_offset() const {
        return {float(offset_column_) * Grid::cell_size, float(offset_row_) * Grid::cell_size};
    }
    /// Terrain of window cell (row, column), resident or not: 1 for a wall.
    int terrain_cell(int row, int column) const {
        return grid_.terrain_cell(seed_, row + offset_row_, column + offset_column_);
    }
    /// What a teleport to window cell (row, column) meets: the grid inside the resident
    /// area, the terrain outside it (any row and column).
    int teleport_cell(int row, int column) const {
        if (row >= resident_cells_[0] && column >= resident_cells_[1] && row <= resident_cells_[2] &&
            column <= resident_cells_[3] && row >= 0 && column >= 0 && row < Grid::rows &&
            column < Grid::columns)
            return grid_.cell(row, column);
        return terrain_cell(row, column);
    }
    /// Keeps the population around `center` (the player): updates the resident cells to the
    /// `retained_half` rectangle, retires enemies outside it, freezes those outside
    /// `active_half`, and spawns at most one enemy per call outside the view (`normal_half`)
    /// when the fuzzy spawn rate has earned a credit. Half extents in legacy pixels;
    /// `elapsed` is simulated seconds, zero while paused. Throws std::invalid_argument for a
    /// negative or nonfinite `elapsed`.
    void stream_enemies(kinetra::Vec2 center, kinetra::Vec2 normal_half, kinetra::Vec2 active_half,
                        kinetra::Vec2 retained_half, float elapsed = 0);
    /// Moves the crates over `delta` updates and resolves their contacts with the player,
    /// the enemies, each other and the walls.
    void update_objects(float delta);
    /// The crates: dynamic bodies without AI, one per loaded room.
    const auto &get_objects() const { return props_; }

    /// Seconds per decision tick: 1/20.
    float get_tick_rate() const;

    void set_player(dotio::Player player);

    dotio::Player &get_player();

    void set_ghost(dotio::Player ghost);

    /// The ghost: the free camera of Debug Mode, moved like a player but not simulated.
    dotio::Player &get_ghost();

    /// Fills the next pool slot (see Tracker::add_instance).
    void add_enemy(kinetra::Vec2 position, kinetra::Vec2 target, const float fear);

    void add_arrow(kinetra::Vec2 position, kinetra::Vec2 velocity);

    void add_bolt(const kinetra::Vec2 position, const kinetra::Vec2 velocity);

    /// One update of every simulated enemy: target, passing, movement, wall collision.
    /// `debug` is the invulnerable debug mode.
    void update_enemies(const float delta, const bool debug);

    /// Whether enemies that follow a route (pursue, recover, path wander) pass each other
    /// head-on in agreed side lanes (kinetra::PassingCoordinator). Off in a new world;
    /// the player's death keeps it.
    bool passing() const { return passing_; }
    /// Turns passing on or off from the next update. Turning it off ends every plan, and its
    /// participants plan their routes again.
    void set_passing(bool passing);
    /// The active passing plans. Mover indices are pool slots; the player is the index
    /// after the last slot.
    std::vector<kinetra::PassingPlanState> passing_plans() const { return passing_coordinator_.plans(); }
    /// What happened to passing plans in the last update, in order, with the indices of
    /// passing_plans().
    std::span<const kinetra::PassingEvent> passing_events() const { return passing_events_; }

    /// Moves the arrows and stops each at the first wall, crate or player hit; a player hit
    /// costs 10 health (none in `debug`), and resets the world at zero.
    void update_arrows(const float delta, const bool debug);

    /// Moves the bolts and stops each at the first wall, crate or enemy hit, sweeping the
    /// enemies' own movement since the last update; a lethal hit rewards the player.
    void update_bolts(const float delta);

    /// Draws enemies, arrows, bolts and the player. `after_enemy`, when set, runs right after
    /// each active enemy is drawn and before the next one, so a presentation can draw over
    /// that enemy alone.
    void draw(dotio::Renderer &renderer, const std::function<void(Enemy &)> &after_enemy = {});

    [[nodiscard]] Tracker<Enemy> &get_enemies();

    [[nodiscard]] Tracker<Arrow> &get_arrows();

    [[nodiscard]] Tracker<Bolt> &get_bolts();

    /// The occupancy raster of this world's resident window. initialize() generates it and
    /// streaming keeps it current; tests may write it to build an arena.
    dotio::Grid &grid() { return grid_; }
    const dotio::Grid &grid() const { return grid_; }
};
} // namespace dotio
