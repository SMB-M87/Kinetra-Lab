#ifndef WORLD_H
#define WORLD_H

#include "arrow.h"
#include "bolt.h"
#include "difficulty.h"
#include "enemy.h"
#include "graph.h"
#include "player.h"
#include "tracker.h"

#include <array>
#include <random>
#include <unordered_set>

namespace logic {
/// Own the streamed-world game population, projectiles and navigation graph.
/// Addresses stay stable because enemies and their state objects reference this world.
class World {
    float _tickRate{1.0f / 20.0f};
    logic::Player _player;
    logic::Player _ghost;
    TrackerData<Enemy, 666> _enemies;
    TrackerData<Arrow, 666> _arrows;
    TrackerData<Bolt, 20> _bolts;
    logic::Graph _graph{};
    std::array<gmath::Position, 666> previous_enemy_positions_{};
    bool enemies_moved_{};
    float spawn_credit_{};
    double survival_seconds_{};
    Difficulty difficulty_{Difficulty::medium};
    std::uint32_t seed_{};
    std::mt19937 population_random_{0};
    int offset_row_{}, offset_column_{};
    void RefreshGraphCell(int row, int column);
    std::array<int, 4> resident_cells_{-1, -1, -1, -1};
    struct Prop {
        int row, column; // Stable absolute spawn district, independent of body displacement.
        kinetra::Body body;
    };
    std::vector<Prop> props_;
    void PopulateObjects();
    std::optional<std::pair<std::size_t, float>> FirstObjectHit(gmath::Segment segment, float radius) const;

  public:
    explicit World(Difficulty difficulty = Difficulty::medium) : difficulty_(difficulty) {
        settings_for(difficulty);
    }
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    std::unordered_map<gmath::GridEntry, std::unordered_set<logic::PhysicsObject *>> EnemyPartitionMap;

    void Initialize(std::uint32_t seed = std::random_device{}(), Difficulty difficulty = Difficulty::medium);
    const DifficultySettings &difficulty_settings() const { return settings_for(difficulty_); }

    void Reset();
    double survival_seconds() const { return survival_seconds_; }
    /// Reach the activity rectangle after five unpaused simulated minutes.
    gmath::Vec2 SpawnHalf(gmath::Vec2 normal_half, gmath::Vec2 active_half) const {
        const float progress = float(std::clamp(survival_seconds_ / 300.0, 0.0, 1.0));
        const auto initial = normal_half * 1.75f;
        return {std::min(active_half.X, initial.X + (active_half.X - initial.X) * progress),
                std::min(active_half.Y, initial.Y + (active_half.Y - initial.Y) * progress)};
    }
    /// Shift the local navigation/physics window before the player reaches its edge.
    gmath::Vec2 StreamAroundPlayer();
    gmath::Vec2 WorldOffset() const {
        return {offset_column_ * Grid::CellSize, offset_row_ * Grid::CellSize};
    }
    int TerrainCell(int row, int column) const {
        return Grid::TerrainCell(seed_, row + offset_row_, column + offset_column_);
    }
    int TeleportCell(int row, int column) const {
        if (row >= resident_cells_[0] && column >= resident_cells_[1] && row <= resident_cells_[2] &&
            column <= resident_cells_[3] && row >= 0 && column >= 0 && row < Grid::Rows &&
            column < Grid::Columns)
            return Grid::Array[row][column];
        return TerrainCell(row, column);
    }
    /// Update residency; timed fuzzy density control refills at most one offscreen slot per call.
    /// elapsed is simulated seconds, zero while paused; nearby counts ignore health and mana.
    void StreamEnemies(gmath::Position center, gmath::Vec2 normal_half, gmath::Vec2 active_half,
                       gmath::Vec2 retained_half, float elapsed = 0);
    void UpdateObjects(float delta);
    const auto &GetObjects() const { return props_; }

    float GetTickRate() const;

    void ExtractGraph();

    void SetPlayer(logic::Player player);

    logic::Player &GetPlayer();

    void SetGhost(logic::Player ghost);

    logic::Player &GetGhost();

    void AddEnemy(gmath::Position position, gmath::Position target, const float fear, bool active);

    void AddArrow(gmath::Position position, gmath::Vector2 velocity);

    void AddBolt(const gmath::Position position, const gmath::Vector2 velocity);

    void UpdateEnemies(const float delta, const bool debug);

    void UpdateArrows(const float delta, const bool debug);

    void UpdateBolts(const float delta);

    void Draw(gamef::Renderer &renderer);

    [[nodiscard]] Tracker<Enemy> &GetEnemies();

    [[nodiscard]] Tracker<Arrow> &GetArrows();

    [[nodiscard]] Tracker<Bolt> &GetBolts();

    [[nodiscard]] logic::Graph &GetGraph();
};
} // namespace logic

#endif
