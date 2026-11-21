#ifndef WORLD_H
#define WORLD_H

#include "dotio/ai/tracker.h"
#include "dotio/difficulty.h"
#include "dotio/entities/arrow.h"
#include "dotio/entities/bolt.h"
#include "dotio/entities/enemy.h"
#include "dotio/entities/player.h"
#include "dotio/navigation/graph.h"

#include <array>
#include <random>
#include <unordered_set>

namespace dotio {
/// Own the streamed-world game population, projectiles and navigation graph.
/// Addresses stay stable because enemies and their state objects reference this world.
class World {
    float _tickRate{1.0f / 20.0f};
    dotio::Player _player;
    dotio::Player _ghost;
    TrackerData<Enemy, 666> _enemies;
    TrackerData<Arrow, 666> _arrows;
    TrackerData<Bolt, 20> _bolts;
    dotio::Grid grid_;
    dotio::Graph _graph{};
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
    void RefreshGraphCell(int row, int column);
    std::array<int, 4> resident_cells_{-1, -1, -1, -1};
    struct Prop {
        int row, column; // Stable absolute spawn district, independent of body displacement.
        kinetra::Body body;
    };
    std::vector<Prop> props_;
    void PopulateObjects();
    std::optional<std::pair<std::size_t, float>> FirstObjectHit(dotio::math::Segment segment,
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
    void Initialize(std::uint32_t seed, Difficulty difficulty = Difficulty::medium);
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
    void Reset();
    /// Number of Reset() calls so far. A reset moves the player to a new spawn point, so
    /// drawing must not blend across one.
    std::uint64_t ResetCount() const { return resets_; }
    double survival_seconds() const { return survival_seconds_; }
    /// Reach the activity rectangle after five unpaused simulated minutes.
    dotio::math::Vec2 SpawnHalf(dotio::math::Vec2 normal_half, dotio::math::Vec2 active_half) const {
        const float progress = float(std::clamp(survival_seconds_ / 300.0, 0.0, 1.0));
        const auto initial = normal_half * 1.75f;
        return {std::min(active_half.X, initial.X + (active_half.X - initial.X) * progress),
                std::min(active_half.Y, initial.Y + (active_half.Y - initial.Y) * progress)};
    }
    /// Shift the local navigation/physics window before the player reaches its edge.
    dotio::math::Vec2 StreamAroundPlayer();
    dotio::math::Vec2 WorldOffset() const {
        return {float(offset_column_) * Grid::CellSize, float(offset_row_) * Grid::CellSize};
    }
    int TerrainCell(int row, int column) const {
        return grid_.TerrainCell(seed_, row + offset_row_, column + offset_column_);
    }
    int TeleportCell(int row, int column) const {
        if (row >= resident_cells_[0] && column >= resident_cells_[1] && row <= resident_cells_[2] &&
            column <= resident_cells_[3] && row >= 0 && column >= 0 && row < Grid::Rows &&
            column < Grid::Columns)
            return grid_.Array[row][column];
        return TerrainCell(row, column);
    }
    /// Update residency; timed fuzzy density control refills at most one offscreen slot per call.
    /// elapsed is simulated seconds, zero while paused; nearby counts ignore health and mana.
    void StreamEnemies(dotio::math::Position center, dotio::math::Vec2 normal_half,
                       dotio::math::Vec2 active_half, dotio::math::Vec2 retained_half, float elapsed = 0);
    void UpdateObjects(float delta);
    const auto &GetObjects() const { return props_; }

    float GetTickRate() const;

    void ExtractGraph();

    void SetPlayer(dotio::Player player);

    dotio::Player &GetPlayer();

    void SetGhost(dotio::Player ghost);

    dotio::Player &GetGhost();

    void AddEnemy(dotio::math::Position position, dotio::math::Position target, const float fear,
                  bool active);

    void AddArrow(dotio::math::Position position, dotio::math::Vector2 velocity);

    void AddBolt(const dotio::math::Position position, const dotio::math::Vector2 velocity);

    void UpdateEnemies(const float delta, const bool debug);

    void UpdateArrows(const float delta, const bool debug);

    void UpdateBolts(const float delta);

    void Draw(dotio::Renderer &renderer);

    [[nodiscard]] Tracker<Enemy> &GetEnemies();

    [[nodiscard]] Tracker<Arrow> &GetArrows();

    [[nodiscard]] Tracker<Bolt> &GetBolts();

    [[nodiscard]] dotio::Graph &GetGraph();

    /// The occupancy raster of this world's resident window. Initialize() generates it and
    /// streaming keeps it current; tests may write it to build an arena.
    dotio::Grid &grid() { return grid_; }
    const dotio::Grid &grid() const { return grid_; }
};
} // namespace dotio

#endif
