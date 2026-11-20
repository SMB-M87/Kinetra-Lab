#pragma once

#include "dotio/entities/physics_adapter.h"
#include "dotio/world.h"
#include "dotio/zoom.h"
#include "kinetra_lab/debug.h"
#include <array>
#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/timing.h>
#include <kinetra/view/camera.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dotio {

inline kinetra::Vec2 to_world(dotio::math::Vec2 point) {
    return {point.X / 25, -point.Y / 25};
}
inline dotio::math::Vec2 from_world(kinetra::Vec2 point) {
    return {point.x * 25, -point.y * 25};
}

/// The color an enemy and its debug drawings have in each AI state; the PATHS parts of the
/// debug menu show them as swatches.
namespace state_color {
inline constexpr kinetra::Color wander{0, 0.5f, 1}, pursue{1, 0.65f, 0}, attack{1, 0, 0}, recover{0, 1, 0},
    kamikaze{1, 0.3f, 0.3f};
} // namespace state_color

/// The colors of the debug layers that are drawn in one color, for their menu rows.
namespace layer_color {
inline constexpr kinetra::Color vectors{0.8f, 1, 0.3f}, collisions{0.6f, 0.8f, 1}, active_agents{1, 1, 0},
    hiding_hits{0, 1, 0};
} // namespace layer_color

/// Limits of the tuning rows, as {minimum, maximum, step, whether the step is a factor}.
/// They stay within what dotio::validate() accepts for the values they change.
namespace tuning_step {
inline constexpr kinetra::DebugStep movement{0.25f, dotio::max_movement_multiplier, 1.25f, true},
    fire_interval{0.2f, 4, 1.25f, true}, projectile_speed{4, 30, 1}, prediction{0, 1.5f, 0.05f},
    state_force{0, dotio::max_state_force, 0.05f}, spawn_rate{0, 8, 0.25f}, crowd_load{0, 2, 0.1f},
    spawn_spacing{100, 600, 25};
/// Limits of the whole-number rows: pursuit cycles, hits per enemy and the active limit.
inline constexpr int fewest_pursuit_cycles = 1, most_pursuit_cycles = 40, fewest_hits = 1, most_hits = 5,
                     smallest_spawn_limit = 0, largest_spawn_limit = 40;
} // namespace tuning_step

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
    std::unique_ptr<dotio::World> world_ = std::make_unique<dotio::World>();
    kinetra::FixedClock clock_{1.0 / 60, 8};
    int tick_{};
    /// moving_positions() before and after the last tick; presentation only.
    std::vector<std::optional<dotio::math::Vec2>> before_tick_, after_tick_;
    /// Whether the previous frame was paused; the first running frame drops the old snapshots.
    bool was_paused_{};
    /// Set by the SINGLE TICK row; the next advance() runs one tick of the paused world.
    bool step_requested_{};

    /// A check row for a debug layer that is drawn in one color.
    static kinetra::DebugEntry layer_row(std::string label, bool &flag, kinetra::Color color, int depth = 1) {
        auto row = kinetra::debug_check(std::move(label), flag, depth);
        row.swatch = color;
        return row;
    }

    void add_simulation_entries(std::vector<kinetra::DebugEntry> &entries) {
        entries.push_back({.label = "RUNNING",
                           .action = [this] { paused = !paused; },
                           .kind = kinetra::DebugEntryKind::check,
                           .active = [this] { return !paused; },
                           .depth = 1});
        entries.push_back({.label = "SINGLE TICK",
                           .action =
                               [this] {
                                   // A tick is only single in a world that stands still.
                                   paused = true;
                                   step_requested_ = true;
                               },
                           .depth = 1});
        entries.push_back(kinetra_lab::time_scale_entry(time_scale));
        entries.push_back(kinetra::debug_check("INVULNERABLE", invulnerable));
    }

    void add_layer_entries(std::vector<kinetra::DebugEntry> &entries) {
        entries.push_back(layer_row("VECTORS", vectors, layer_color::vectors));
        entries.push_back(kinetra::debug_check("STEERING PROBES", probes));
        entries.push_back(kinetra::debug_check("PATHS", paths[0]));
        // As in i4sim, a layer lists its parts only while it is on. Each part is drawn in
        // the color of its AI state.
        if (paths[0]) {
            entries.push_back(layer_row("WANDER", paths[1], state_color::wander, 2));
            entries.push_back(layer_row("PURSUE", paths[2], state_color::pursue, 2));
            entries.push_back(layer_row("KAMIKAZE", paths[3], state_color::kamikaze, 2));
            entries.push_back(layer_row("RECOVERY", paths[4], state_color::recover, 2));
        }
        entries.push_back(layer_row("COLLISION SHAPES", collisions, layer_color::collisions));
        entries.push_back(layer_row("ACTIVE AGENTS", active_agents, layer_color::active_agents));
        entries.push_back(kinetra::debug_check("AI STATES", ai));
        entries.push_back(layer_row("HIDING HITS", paths[5], layer_color::hiding_hits));
    }

    void add_enemy_entries(std::vector<kinetra::DebugEntry> &entries) {
        using kinetra::debug_number;
        auto &settings = world_->settings();
        auto &tuning = world_->enemy_tuning();
        const auto wander_name = [](dotio::WanderMode mode) {
            return mode == dotio::WanderMode::path ? "PATH" : "STEERING";
        };
        entries.push_back(
            folds.list("WANDER", [&tuning, wander_name] { return std::string(wander_name(tuning.wander)); }));
        if (folds.list_open("WANDER"))
            for (const auto mode : {dotio::WanderMode::steering, dotio::WanderMode::path})
                entries.push_back({.label = wander_name(mode),
                                   .action =
                                       [this, &tuning, mode] {
                                           tuning.wander = mode;
                                           folds.close_list();
                                       },
                                   .kind = kinetra::DebugEntryKind::choice,
                                   .active = [&tuning, mode] { return tuning.wander == mode; },
                                   .depth = 2});
        entries.push_back(debug_number("MOVE SPEED", settings.movement_multiplier, tuning_step::movement));
        entries.push_back(
            debug_number("FIRE INTERVAL", settings.fire_interval_multiplier, tuning_step::fire_interval));
        entries.push_back(
            debug_number("ARROW SPEED", settings.projectile_speed, tuning_step::projectile_speed));
        entries.push_back(debug_number("PREDICTION", settings.prediction, tuning_step::prediction));
        entries.push_back(kinetra::debug_count("PURSUIT CYCLES", settings.pursuit_cycles,
                                               tuning_step::fewest_pursuit_cycles,
                                               tuning_step::most_pursuit_cycles));
        entries.push_back(kinetra::debug_count("HITS", settings.enemy_hits, tuning_step::fewest_hits,
                                               tuning_step::most_hits));
        entries.push_back(debug_number("WANDER FORCE", tuning.wander_force, tuning_step::state_force));
        entries.push_back(debug_number("ATTACK FORCE", tuning.attack_force, tuning_step::state_force));
        entries.push_back(debug_number("PURSUE FORCE", tuning.pursue_force, tuning_step::state_force));
        entries.push_back(debug_number("RECOVER FORCE", tuning.recover_force, tuning_step::state_force));
        entries.push_back(debug_number("KAMIKAZE FORCE", tuning.kamikaze_force, tuning_step::state_force));
    }

    void add_spawning_entries(std::vector<kinetra::DebugEntry> &entries) {
        auto &settings = world_->settings();
        entries.push_back(kinetra::debug_count("ACTIVE LIMIT", settings.active_spawn_limit,
                                               tuning_step::smallest_spawn_limit,
                                               tuning_step::largest_spawn_limit));
        entries.push_back(
            kinetra::debug_number("SPAWN RATE", settings.spawn_rate_multiplier, tuning_step::spawn_rate));
        entries.push_back(
            kinetra::debug_number("CROWD LOAD", settings.crowd_load_multiplier, tuning_step::crowd_load));
        entries.push_back(
            kinetra::debug_number("SPACING", settings.spawn_spacing, tuning_step::spawn_spacing));
    }

  public:
    bool paused{}, help{}, debug{}, ghost{};
    bool vectors{}, collisions{}, ai{}, probes{}, invulnerable{};
    bool statistics{}, active_agents{}, frame_times{};
    /// Simulated seconds per elapsed second, set by the debug menu's SPEED row.
    float time_scale{1};
    kinetra::DebugMenu menu;
    /// Which section and which list of the debug menu are open.
    kinetra::DebugFolds folds{"SIMULATION"};
    /// The debug menu's ZOOM row and the steps its buttons asked for.
    kinetra_lab::ZoomControl zoom;
    /// Frames, fixed updates and AI decisions per second, for the statistics line.
    kinetra::RateCounter frame_rate, update_rate, decision_rate;
    /// The rows of the debug menu in display order: the headings SIMULATION, LAYERS, ENEMIES
    /// and SPAWNING, each followed by its rows while it is open, then, below a rule, ZOOM,
    /// GHOST CAMERA and RESET PARAMETERS. At most one section is open; SIMULATION is at the
    /// start. PATHS lists its four parts only while it is on.
    ///
    /// The tuning rows change the world's balance (dotio::World::settings) and its enemy
    /// tuning in place, within tuning_step; RESET PARAMETERS restores both and the time
    /// scale. The rows follow the contract of kinetra::DebugEntry: they refer to this
    /// application, which must outlive them.
    std::vector<kinetra::DebugEntry> debug_entries() {
        std::vector<kinetra::DebugEntry> entries;
        for (const std::string label : {"SIMULATION", "LAYERS", "ENEMIES", "SPAWNING"}) {
            entries.push_back(folds.section(label));
            if (!folds.section_open(label))
                continue;
            if (label == "SIMULATION")
                add_simulation_entries(entries);
            else if (label == "LAYERS")
                add_layer_entries(entries);
            else if (label == "ENEMIES")
                add_enemy_entries(entries);
            else
                add_spawning_entries(entries);
        }
        entries.push_back(zoom.entry());
        entries.push_back(kinetra::debug_check("GHOST CAMERA", ghost, 0));
        entries.push_back({.label = "RESET PARAMETERS", .action = [this] {
                               world_->reset_tuning();
                               time_scale = 1;
                           }});
        return entries;
    }
    /// The PATHS layer and its parts: [0] the layer, [1] WANDER, [2] PURSUE, [3] KAMIKAZE,
    /// [4] RECOVERY, and [5] the separate HIDING HITS layer.
    std::array<bool, 6> paths{};
    /// The player and every enemy, arrow and bolt slot, in a fixed order: the objects that
    /// move each tick and that drawing interpolates. Pointers stay valid as long as the world.
    std::vector<dotio::StaticObject *> moving_objects() {
        std::vector<dotio::StaticObject *> result{&world_->GetPlayer()};
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
    std::vector<std::optional<dotio::math::Vec2>> moving_positions() {
        const auto offset = world_->WorldOffset();
        std::vector<std::optional<dotio::math::Vec2>> result;
        for (const auto *object : moving_objects())
            result.push_back(object->IsActive ? std::optional{object->Position + offset} : std::nullopt);
        return result;
    }
    /// moving_positions() before and after the last tick, for drawing between them.
    const std::vector<std::optional<dotio::math::Vec2>> &before_tick() const { return before_tick_; }
    const std::vector<std::optional<dotio::math::Vec2>> &after_tick() const { return after_tick_; }
    /// Fraction of the next tick already accumulated, in [0, 1); one while paused, so a
    /// paused frame shows the exact state.
    double interpolation() const { return paused ? 1.0 : clock_.interpolation(); }
    explicit Application(std::uint32_t seed = std::random_device{}(),
                         dotio::Difficulty difficulty = dotio::Difficulty::medium) {
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
    dotio::World &world() { return *world_; }
    /// Reject destinations overlapping terrain, physical props or active enemies.
    bool teleport_clear(dotio::math::Position point, float radius) {
        if (!std::isfinite(point.X) || !std::isfinite(point.Y) || std::abs(point.X) > 10000000 ||
            std::abs(point.Y) > 10000000)
            return false;
        auto candidate = physics_body(world_->GetPlayer());
        candidate.position = physics_vector(point);
        candidate.radius = radius;
        const auto area = kinetra::bounds(candidate);
        const float cell = dotio::Grid::CellSize;
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
        // The ZOOM row's steps are taken even outside debug mode, so none is left over for
        // the frame that switches it on.
        kinetra::zoom(camera, commands.zoom_steps + zoom.take_steps(),
                      debug ? debug_zoom_limits : zoom_limits);
        zoom.observe(camera);
        if (ghost) {
            const auto pan = kinetra::normalized(commands.camera_pan + commands.direction);
            world_->GetGhost().Position +=
                from_world(pan * (float(std::min(elapsed, 0.1)) * 400.f / camera.pixels_per_unit));
        }
        const auto initial_shift = world_->StreamAroundPlayer();
        const auto center = world_->GetPlayer().Position;
        // A single tick runs in a paused world: exactly one, whatever time has passed.
        const bool single_tick = paused && std::exchange(step_requested_, false);
        const bool running = !paused || single_tick;
        // Lifecycle bounds do not shrink when the player zooms in, avoiding spawn churn.
        const int scaled_steps = clock_.advance(elapsed * static_cast<double>(time_scale));
        const int steps = single_tick ? 1 : scaled_steps;
        // Legacy coordinates use 25 pixels per world unit: 0.9 * 25 / (2 * 2.5) = 4.5.
        world_->StreamEnemies(center, {camera.viewport.x * 0.5f, camera.viewport.y * 0.5f},
                              {camera.viewport.x * 4.5f, camera.viewport.y * 4.5f},
                              {camera.viewport.x * 6.25f, camera.viewport.y * 6.25f},
                              running ? float(steps * clock_.step()) : 0.f);
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
            player.Heading =
                ghost ? dotio::math::Vec2{} : dotio::math::Vec2{commands.direction.x, -commands.direction.y};
            if (!running)
                continue;
            if (step == steps - 1) {
                before_tick_ = moving_positions();
                resets_before_tick = world_->ResetCount();
            }
            player.Speed = 1;
            player.Update(1);
            update_rate.count();
            aim -= world_->StreamAroundPlayer();
            dotio::Grid::HandleGridCollision(player, 1, &dotio::Grid::Array[0][0]);
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
