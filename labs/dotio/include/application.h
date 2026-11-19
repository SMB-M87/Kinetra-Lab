#pragma once

#include "physics_adapter.h"
#include "world.h"
#include "zoom.h"
#include <array>
#include <kinetra/camera.h>
#include <kinetra/debug_view.h>
#include <kinetra/timing.h>
#include <memory>
#include <optional>
#include <vector>

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
    /// moving_positions() before and after the last tick; presentation only.
    std::vector<std::optional<gmath::Vec2>> before_tick_, after_tick_;
    /// Whether the previous frame was paused; the first running frame drops the old snapshots.
    bool was_paused_{};

  public:
    bool paused{}, help{}, debug{}, ghost{};
    bool vectors{}, collisions{}, ai{}, probes{}, invulnerable{};
    bool statistics{}, active_agents{}, frame_times{};
    kinetra::DebugMenu menu;
    /// Frames, fixed updates and AI decisions per second, for the statistics line.
    kinetra::RateCounter frame_rate, update_rate, decision_rate;
    auto debug_options() {
        std::vector<kinetra::DebugOption> options{{"GHOST CAMERA", &ghost},
                                                  {"VECTORS", &vectors},
                                                  {"STEERING PROBES", &probes},
                                                  {"PATHS", &paths[0]}};
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
    std::array<bool, 6> paths{};
    /// The player and every enemy, arrow and bolt slot, in a fixed order: the objects that
    /// move each tick and that drawing interpolates. Pointers stay valid as long as the world.
    std::vector<logic::StaticObject *> moving_objects() {
        std::vector<logic::StaticObject *> result{&world_->GetPlayer()};
        for (auto &enemy : world_->GetEnemies())
            result.push_back(&enemy);
        for (auto &arrow : world_->GetArrows())
            result.push_back(&arrow);
        for (auto &bolt : world_->GetBolts())
            result.push_back(&bolt);
        return result;
    }
    /// Positions of moving_objects() including the world offset, so a streaming shift between
    /// two snapshots cancels out; empty for an inactive slot.
    std::vector<std::optional<gmath::Vec2>> moving_positions() {
        const auto offset = world_->WorldOffset();
        std::vector<std::optional<gmath::Vec2>> result;
        for (const auto *object : moving_objects())
            result.push_back(object->IsActive ? std::optional{object->Position + offset} : std::nullopt);
        return result;
    }
    /// moving_positions() before and after the last tick, for drawing between them.
    const std::vector<std::optional<gmath::Vec2>> &before_tick() const { return before_tick_; }
    const std::vector<std::optional<gmath::Vec2>> &after_tick() const { return after_tick_; }
    /// Fraction of the next tick already accumulated, in [0, 1); one while paused, so a
    /// paused frame shows the exact state.
    double interpolation() const { return paused ? 1.0 : clock_.interpolation(); }
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
                wall.position = {(float(col) + 0.5f) * cell, (float(row) + 0.5f) * cell};
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
        kinetra::zoom(camera, commands.zoom_steps, debug ? debug_zoom_limits : zoom_limits);
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
        frame_rate.count();
        // Snapshots from before a pause would draw the first running frames behind the state
        // shown while paused; start from the current state instead.
        if (!paused && was_paused_)
            before_tick_ = after_tick_ = moving_positions();
        was_paused_ = paused;
        std::uint64_t resets_before_tick = 0;
        for (int step = 0; step < steps; ++step) {
            auto &player = world_->GetPlayer();
            player.Heading = ghost ? gmath::Vec2{} : gmath::Vec2{commands.direction.x, -commands.direction.y};
            if (paused)
                continue;
            if (step == steps - 1) {
                before_tick_ = moving_positions();
                resets_before_tick = world_->ResetCount();
            }
            player.Speed = 1;
            player.Update(1);
            update_rate.count();
            aim -= world_->StreamAroundPlayer();
            logic::Grid::HandleGridCollision(player, 1, &logic::Grid::Array[0][0]);
            if (commands.shoot && !ghost)
                player.Shoot(aim, debug && invulnerable);
            if (++tick_ == 3) {
                tick_ = 0;
                decision_rate.count();
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
            if (step == steps - 1) {
                after_tick_ = moving_positions();
                // A death respawned the player and cleared the projectiles: nothing to blend.
                if (world_->ResetCount() != resets_before_tick)
                    before_tick_ = after_tick_;
            }
        }
        frame_rate.advance(elapsed);
        update_rate.advance(elapsed);
        decision_rate.advance(elapsed);
        camera.position = to_world(ghost ? world_->GetGhost().Position : world_->GetPlayer().Position);
    }
};
} // namespace dotio
