#pragma once

#include "dotio/ai/tracker.h"
#include "dotio/difficulty.h"
#include "dotio/entities/arrow.h"
#include "dotio/entities/bolt.h"
#include "dotio/entities/enemy.h"
#include "dotio/entities/player.h"
#include "dotio/navigation/grid.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <unordered_set>
#include <vector>

namespace dotio {
/// Own the streamed-world game population, projectiles and grid.
/// Addresses stay stable because enemies and their state objects reference this world.
class World {
    float tickRate_{1.0f / 20.0f};
    dotio::Player player_;
    dotio::Player ghost_;
    TrackerData<Enemy, 666> enemies_;
    TrackerData<Arrow, 666> arrows_;
    TrackerData<Bolt, 20> bolts_;
    dotio::Grid grid_;
    std::array<dotio::math::Position, 666> previous_enemy_positions_{};
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
    void populate_objects();
    std::optional<std::pair<std::size_t, float>> first_object_hit(dotio::math::Segment segment,
                                                                  float radius) const;

  public:
    /// Throws std::invalid_argument for a difficulty without a preset.
    explicit World(Difficulty difficulty = Difficulty::medium)
        : difficulty_(difficulty), settings_(settings_for(difficulty)) {}
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    std::unordered_map<dotio::math::GridEntry, std::unordered_set<dotio::PhysicsObject *>> EnemyPartitionMap;

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
    /// keep them within what validate(const DifficultySettings &) accepts. Reset() keeps
    /// them, reset_tuning() and Initialize() restore the preset.
    DifficultySettings &settings() { return settings_; }
    /// The wander mode and state forces of this world's enemies, read on every update.
    /// Values are not checked here; keep them within what validate(const EnemyTuning &)
    /// accepts. Reset() keeps them, reset_tuning() and Initialize() restore the defaults.
    EnemyTuning &enemy_tuning() { return enemy_tuning_; }
    const EnemyTuning &enemy_tuning() const { return enemy_tuning_; }
    /// Restores settings() to the preset of the difficulty and enemy_tuning() to its defaults.
    void reset_tuning();
    /// Generator behind steering wander: the headings enemies start with and their drift.
    /// Seeded from the world's seed in Initialize(), so the same seed and the same calls
    /// give the same wander. Only the wander code draws from it.
    std::mt19937 &wander_random() { return wander_random_; }
    /// Generator behind enemy fear (on spawn, reset and reload) and the targets and jitter of
    /// path-mode wander. Seeded from the world's seed in Initialize().
    std::mt19937 &behavior_random() { return behavior_random_; }

    /// Starts a new life after the player's death: respawns the player and retires every
    /// enemy and projectile. Difficulty, settings() and enemy_tuning() stay as they are.
    void reset();
    /// Number of Reset() calls so far. A reset moves the player to a new spawn point, so
    /// drawing must not blend across one.
    std::uint64_t reset_count() const { return resets_; }
    double survival_seconds() const { return survival_seconds_; }
    /// Reach the activity rectangle after five unpaused simulated minutes.
    dotio::math::Vec2 spawn_half(dotio::math::Vec2 normal_half, dotio::math::Vec2 active_half) const {
        const float progress = float(std::clamp(survival_seconds_ / 300.0, 0.0, 1.0));
        const auto initial = normal_half * 1.75f;
        return {std::min(active_half.X, initial.X + (active_half.X - initial.X) * progress),
                std::min(active_half.Y, initial.Y + (active_half.Y - initial.Y) * progress)};
    }
    /// Shift the local navigation/physics window before the player reaches its edge.
    dotio::math::Vec2 stream_around_player();
    dotio::math::Vec2 world_offset() const {
        return {float(offset_column_) * Grid::cell_size, float(offset_row_) * Grid::cell_size};
    }
    int terrain_cell(int row, int column) const {
        return grid_.terrain_cell(seed_, row + offset_row_, column + offset_column_);
    }
    int teleport_cell(int row, int column) const {
        if (row >= resident_cells_[0] && column >= resident_cells_[1] && row <= resident_cells_[2] &&
            column <= resident_cells_[3] && row >= 0 && column >= 0 && row < Grid::rows &&
            column < Grid::columns)
            return grid_.cell(row, column);
        return terrain_cell(row, column);
    }
    /// Update residency; timed fuzzy density control refills at most one offscreen slot per call.
    /// elapsed is simulated seconds, zero while paused; nearby counts ignore health and mana.
    void stream_enemies(dotio::math::Position center, dotio::math::Vec2 normal_half,
                        dotio::math::Vec2 active_half, dotio::math::Vec2 retained_half, float elapsed = 0);
    void update_objects(float delta);
    const auto &get_objects() const { return props_; }

    float get_tick_rate() const;

    void set_player(dotio::Player player);

    dotio::Player &get_player();

    void set_ghost(dotio::Player ghost);

    dotio::Player &get_ghost();

    void add_enemy(dotio::math::Position position, dotio::math::Position target, const float fear);

    void add_arrow(dotio::math::Position position, dotio::math::Vector2 velocity);

    void add_bolt(const dotio::math::Position position, const dotio::math::Vector2 velocity);

    void update_enemies(const float delta, const bool debug);

    void update_arrows(const float delta, const bool debug);

    void update_bolts(const float delta);

    void draw(dotio::Renderer &renderer);

    [[nodiscard]] Tracker<Enemy> &get_enemies();

    [[nodiscard]] Tracker<Arrow> &get_arrows();

    [[nodiscard]] Tracker<Bolt> &get_bolts();

    /// The occupancy raster of this world's resident window. Initialize() generates it and
    /// streaming keeps it current; tests may write it to build an arena.
    dotio::Grid &grid() { return grid_; }
    const dotio::Grid &grid() const { return grid_; }
};
} // namespace dotio
