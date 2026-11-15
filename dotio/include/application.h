#pragma once

#include "physics_adapter.h"
#include "world.h"
#include <array>
#include <memory>
#include <kinetra/camera.h>
#include <kinetra/debug_view.h>
#include <kinetra/timing.h>

namespace dotio {

inline kinetra::Vec2 to_world(gmath::Vec2 point) {
    return {point.X / 25, -point.Y / 25};
}
inline gmath::Vec2 from_world(kinetra::Vec2 point) {
    return {point.x * 25, -point.y * 25};
}

struct Commands {
    kinetra::Vec2 direction{}, camera_pan{}, aim{};
    bool shoot{}, pause{}, help{}, debug{}, ghost{}, teleport{};
    float zoom_steps{};
    kinetra::Vec2 teleport_target{};
    std::array<bool, 6> paths{};
};

/// Own the ported world at a stable address: enemies and state objects reference it.
/// Legacy movement uses 60-Hz frame units; AI decisions run at the original 20 Hz,
/// now sequentially on the main thread rather than racing the update/render loop.
class Application {
    std::unique_ptr<logic::World> world_ = std::make_unique<logic::World>();
    kinetra::FixedClock clock_{1.0 / 60, 8};
    int tick_{};
    double statistics_elapsed_{};
    int frames_{}, updates_{}, decisions_{};

  public:
    bool paused{}, help{}, debug{}, ghost{};
    bool vectors{}, collisions{}, ai{}, probes{}, invulnerable{};
    kinetra::DebugMenu menu;
    auto debug_options() {
        std::vector<kinetra::DebugOption> options{{"STATISTICS", &statistics},  {"DEBUG MODE", &debug},
                                                  {"GHOST CAMERA", &ghost},     {"VECTORS", &vectors},
                                                  {"STEERING PROBES", &probes}, {"PATHS", &paths[0]}};
        if (paths[0]) {
            options.push_back({"  WANDER", &paths[1]});
            options.push_back({"  PURSUE", &paths[2]});
            options.push_back({"  KAMIKAZE", &paths[3]});
            options.push_back({"  RECOVERY", &paths[4]});
        }
        options.insert(options.end(), {{"COLLISION SHAPES", &collisions},
                                       {"ACTIVE AGENTS", &active_agents},
                                       {"AI STATES", &ai},
                                       {"HIDING HITS", &paths[5]},
                                       {"INVULNERABLE", &invulnerable}});
        return options;
    }
    bool statistics{}, active_agents{};
    double fps{}, ups{}, tps{};
    std::array<bool, 6> paths{};
    explicit Application(std::uint32_t seed = std::random_device{}(),
                         logic::Difficulty difficulty = logic::Difficulty::medium) {
        world_->Initialize(seed, difficulty);
    }
    struct AgentCounts {
        int loaded{}, active{};
    };
    /// Count resident live enemies, including frozen residents; unused pool slots are excluded.
    AgentCounts agent_counts() const {
        AgentCounts counts;
        for (const auto &enemy : world_->GetEnemies())
            if (enemy.IsActive) {
                ++counts.loaded;
                counts.active += !enemy.Frozen;
            }
        return counts;
    }
    logic::World &world() { return *world_; }
    /// Reject destinations overlapping terrain, physical props or active enemies.
    bool teleport_clear(gmath::Position point, float radius) {
        if (!std::isfinite(point.X) || !std::isfinite(point.Y) || std::abs(point.X) > 10000000 ||
            std::abs(point.Y) > 10000000)
            return false;
        auto candidate = physics_body(world_->GetPlayer());
        candidate.position = physics_vector(point);
        candidate.radius = radius;
        const auto area = kinetra::bounds(candidate);
        const float cell = logic::Grid::CellSize;
        const int first_row = int(std::floor(area.min.y / cell));
        const int last_row = int(std::floor(area.max.y / cell));
        const int first_col = int(std::floor(area.min.x / cell));
        const int last_col = int(std::floor(area.max.x / cell));
        for (int row = first_row; row <= last_row; ++row)
            for (int col = first_col; col <= last_col; ++col) {
                if (!world_->TeleportCell(row, col))
                    continue;
                kinetra::Body wall;
                wall.shape = kinetra::Shape::box;
                wall.position = {(col + 0.5f) * cell, (row + 0.5f) * cell};
                wall.half_extents = {cell / 2, cell / 2};
                if (kinetra::collide(candidate, wall))
                    return false;
            }
        for (const auto &enemy : world_->GetEnemies())
            if (enemy.IsActive && kinetra::collide(candidate, physics_body(enemy)))
                return false;
        for (const auto &object : world_->GetObjects())
            if (kinetra::collide(candidate, object.body))
                return false;
        return true;
    }

    void advance(double elapsed, const Commands &commands, kinetra::Camera2D &camera) {
        if (commands.pause)
            paused = !paused;
        if (commands.help)
            help = !help;
        if (commands.debug)
            debug = !debug;
        if (debug && (commands.ghost || (ghost && commands.teleport))) {
            if (commands.teleport) {
                auto &player = world_->GetPlayer();
                const auto candidate = from_world(commands.teleport_target);
                if (teleport_clear(candidate, player.Radius)) {
                    player.Position = candidate;
                    player.Velocity = {};
                }
            } else {
                ghost = !ghost;
                world_->GetGhost().Position = world_->GetPlayer().Position;
                world_->GetGhost().Velocity = {};
            }
        }
        if (debug)
            for (std::size_t i = 0; i < paths.size(); ++i)
                if (commands.paths[i])
                    paths[i] = !paths[i];
        if (!debug)
            ghost = false;
        const float zoom = commands.zoom_steps;
        camera.pixels_per_unit =
            std::clamp(camera.pixels_per_unit * std::pow(1.1f, zoom), debug ? 2.5f : 25.f, 50.f);
        if (ghost) {
            const auto pan = kinetra::normalized(commands.camera_pan + commands.direction);
            world_->GetGhost().Position +=
                from_world(pan * (float(std::min(elapsed, 0.1)) * 400.f / camera.pixels_per_unit));
        }
        const auto initial_shift = world_->StreamAroundPlayer();
        const auto center = world_->GetPlayer().Position;
        // Lifecycle bounds do not shrink when the player zooms in, avoiding spawn churn.
        const int steps = clock_.advance(elapsed);
        // Legacy coordinates use 25 pixels per world unit: 0.9 * 25 / (2 * 2.5) = 4.5.
        world_->StreamEnemies(center, {camera.viewport.x * 0.5f, camera.viewport.y * 0.5f},
                              {camera.viewport.x * 4.5f, camera.viewport.y * 4.5f},
                              {camera.viewport.x * 6.25f, camera.viewport.y * 6.25f},
                              paused ? 0.f : float(steps * clock_.step()));
        auto aim = from_world(commands.aim) - initial_shift;
        statistics_elapsed_ += elapsed;
        ++frames_;
        for (int step = 0; step < steps; ++step) {
            auto &player = world_->GetPlayer();
            player.Heading = ghost ? gmath::Vec2{} : gmath::Vec2{commands.direction.x, -commands.direction.y};
            if (paused)
                continue;
            player.Speed = 1;
            player.Update(1);
            ++updates_;
            aim -= world_->StreamAroundPlayer();
            logic::Grid::HandleGridCollision(player, 1, &logic::Grid::Array[0][0]);
            if (commands.shoot && !ghost)
                player.Shoot(aim, debug && invulnerable);
            if (++tick_ == 3) {
                tick_ = 0;
                ++decisions_;
                for (auto &enemy : world_->GetEnemies())
                    if (enemy.IsActive && !enemy.Frozen) {
                        enemy.Debug = debug && invulnerable;
                        enemy.Tick();
                    }
            }
            world_->UpdateEnemies(1, debug && invulnerable);
            world_->UpdateObjects(1);
            world_->UpdateBolts(1);
            world_->UpdateArrows(1, debug && invulnerable);
            // Clamp rewards from contact kills as well as projectile kills.
            player.Health = std::clamp(player.Health, 0.f, player.MaxHealth);
            player.Mana = std::clamp(player.Mana, 0.f, player.MaxMana);
        }
        if (statistics_elapsed_ >= 0.5) {
            fps = frames_ / statistics_elapsed_;
            ups = updates_ / statistics_elapsed_;
            tps = decisions_ / statistics_elapsed_;
            statistics_elapsed_ = 0;
            frames_ = updates_ = decisions_ = 0;
        }
        camera.position = to_world(ghost ? world_->GetGhost().Position : world_->GetPlayer().Position);
    }
};
} // namespace dotio
