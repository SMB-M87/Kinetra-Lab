#include "dotio/presentation/presentation.h"
#include "dotio/ai/wander.h"
#include "dotio/entities/physics_adapter.h"
#include "kinetra_lab/camera.h"
#include "kinetra_lab/input.h"
#include "kinetra_lab/inspector.h"
#include "kinetra_lab/route.h"
#include <kinetra/runtime/help_view.h>

namespace dotio {
using namespace kinetra;
Color wall_color(int absolute_row, int absolute_column) {
    // Echo the original row/column gradient without dark corners or hard color resets.
    return {0.32f + 0.12f * std::sin(static_cast<float>(absolute_row) * 0.055f),
            0.36f + 0.14f * std::sin(static_cast<float>(absolute_column) * 0.055f + 2.094f),
            float(0.42 + 0.15 * std::sin((double(absolute_row) + absolute_column) * 0.035 + 4.189))};
}

Commands read_commands(const Input &input, const Camera2D &camera) {
    Commands commands;
    commands.direction = kinetra_lab::direction(input);
    commands.camera_pan = commands.direction;
    commands.aim = camera.screen_to_world(input.mouse);
    commands.shoot = input.mouse_down || input.clicked;

    commands.teleport = input.clicked;
    commands.teleport_target = commands.aim;

    commands.zoom_steps = input.wheel;

    return commands;
}

namespace {
class RuntimeRenderer final : public dotio::Renderer {
    Runtime &runtime_;
    Vec2 half_view_;

    // Reject whole primitive bounds before circle tessellation and vertex uploads.
    // Keep edge-crossing circles and segments even when their centers are offscreen.
    bool visible(Vec2 center, Vec2 half) const {
        const auto delta = center - runtime_.camera.position;
        return std::abs(delta.x) <= half_view_.x + half.x && std::abs(delta.y) <= half_view_.y + half.y;
    }

  public:
    explicit RuntimeRenderer(Runtime &runtime)
        : runtime_(runtime), half_view_(runtime.camera.viewport / (2 * runtime.camera.pixels_per_unit)) {}
    void draw_circle(kinetra::Vec2 center, float radius, kinetra::Color color) override {
        const auto position = to_world(center);
        const float world_radius = radius / 25;
        if (visible(position, {world_radius, world_radius}))
            runtime_.circle(position, world_radius, color);
    }
    void draw_segment(kinetra::Segment segment, kinetra::Color color) override {
        const auto start = to_world(segment.start), end = to_world(segment.end);
        const Vec2 half{std::abs(end.x - start.x) / 2 + 0.03f, std::abs(end.y - start.y) / 2 + 0.03f};
        if (visible((start + end) / 2, half))
            runtime_.line(start, end, 0.06f, color);
    }
};
Color color_of_state(dotio::Enemy &enemy) {
    return enemy.is_kamikazing()   ? state_color::kamikaze
           : enemy.is_recovering() ? state_color::recover
           : enemy.is_pursueing()  ? state_color::pursue
           : enemy.is_attacking()  ? state_color::attack
                                   : state_color::wander;
}
void panel(Runtime &runtime, Vec2 center, Vec2 half, Color color) {
    runtime.box(runtime.camera.screen_to_world(center), half / runtime.camera.pixels_per_unit, 0, color);
}
} // namespace

PresentedPositions::PresentedPositions(Application &application) {
    const auto &before = application.before_tick();
    const auto &after = application.after_tick();
    const auto objects = application.moving_objects();
    const auto offset = application.world().world_offset();
    const auto blend = float(application.interpolation());
    for (std::size_t i = 0; i < objects.size() && i < before.size() && i < after.size(); ++i) {
        auto *object = objects[i];
        if (!object->is_active || !before[i] || !after[i])
            continue;
        // Anything but the last tick moved it (teleport, respawn into the slot): exact.
        const auto absolute = object->position + offset;
        if (absolute != *after[i])
            continue;
        simulated_.emplace_back(object, object->position);
        object->position = {before[i]->x + (after[i]->x - before[i]->x) * blend - offset.x,
                            before[i]->y + (after[i]->y - before[i]->y) * blend - offset.y};
    }
}
PresentedPositions::~PresentedPositions() {
    for (auto &[object, position] : simulated_)
        object->position = position;
}

void render(Runtime &runtime, Application &application, const SettingsOverlay &settings,
            const DebugMenu &menu, const InspectorView *inspector) {
    auto &world = application.world();
    // The camera follows the drawn player, not the simulated one, or the player would jitter
    // on screen; Ghost moves per frame and needs no correction. Restored after the frame.
    const auto simulated_player = to_world(world.get_player().position);
    const auto snapshot = inspector && !settings.visible() ? application.inspect_enemy() : std::nullopt;
    const PresentedPositions presented(application);
    const kinetra_lab::ScopedCameraShift follow_drawn_player(
        runtime.camera,
        application.ghost ? Vec2{} : to_world(world.get_player().position) - simulated_player);
    runtime.begin({0, 0, 0});
    RuntimeRenderer renderer(runtime);
    // Cull generated arena geometry against the active camera.
    const auto top_left = from_world(runtime.camera.screen_to_world({0, 0}));
    const auto bottom_right = from_world(runtime.camera.screen_to_world(runtime.camera.viewport));
    const int first_row = int(std::floor(top_left.y / 100)) - 1;
    const int last_row = int(std::floor(bottom_right.y / 100)) + 1;
    const int first_col = int(std::floor(top_left.x / 100)) - 1;
    const int last_col = int(std::floor(bottom_right.x / 100)) + 1;
    const auto offset = world.world_offset();
    const int row_offset = int(std::lround(offset.y / dotio::Grid::cell_size));
    const int column_offset = int(std::lround(offset.x / dotio::Grid::cell_size));
    for (int row = first_row; row <= last_row; ++row)
        for (int column = first_col; column <= last_col; ++column) {
            const auto center =
                to_world({static_cast<float>(column) * 100.f + 50, static_cast<float>(row) * 100.f + 50});
            if (!world.terrain_cell(row, column))
                continue;
            runtime.box(center, {2, 2}, 0, wall_color(row + row_offset, column + column_offset));
            if (application.debug && application.collisions)
                draw_debug_box(runtime, center, {2, 2}, 0, {0.6f, 0.8f, 1});
        }
    // Every enemy's route goes under the bodies, so it never hides the player, a crate or
    // another enemy; the start of an enemy's own route is drawn over that enemy alone, right
    // after it, so the enemies drawn later cover it again where they overlap.
    const auto shows_path = [&](dotio::Enemy &enemy) {
        const auto &p = application.paths;
        return application.debug && enemy.is_active && p[0] && !enemy.frozen &&
               ((p[1] && enemy.is_wandering()) || (p[2] && enemy.is_pursueing()) ||
                (p[3] && enemy.is_kamikazing()) || (p[4] && enemy.is_recovering()));
    };
    const auto draw_points = [&](std::span<const Vec2> points, Color color) {
        for (std::size_t i = 1; i < points.size(); ++i)
            runtime.line(to_world(points[i - 1]), to_world(points[i]),
                         kinetra_lab::route_line_pixels / runtime.camera.pixels_per_unit, color);
    };
    for (auto &enemy : world.get_enemies())
        if (shows_path(enemy)) {
            std::vector<Vec2> route{enemy.position};
            route.insert(route.end(), enemy.path.remaining().begin(), enemy.path.remaining().end());
            draw_points(route, color_of_state(enemy));
        }
    for (const auto &prop : world.get_objects()) {
        const auto center = to_world(prop.body.position);
        renderer.draw_circle(prop.body.position, prop.body.radius, {0.65f, 0.45f, 0.2f, 1});
        if (application.debug && application.vectors)
            draw_debug_vector(runtime, center, to_world(prop.body.velocity), {1, 0.8f, 0.2f});
        if (application.debug && application.collisions)
            draw_debug_circle(runtime, center, prop.body.radius / 25, {1, 0.8f, 0.2f});
    }
    world.draw(renderer, [&](dotio::Enemy &enemy) {
        if (shows_path(enemy))
            draw_points(kinetra_lab::route_start(enemy.position, enemy.path.remaining(), enemy.radius),
                        color_of_state(enemy));
    });
    if (const auto *enemy = application.selected_enemy())
        draw_debug_circle(runtime, to_world(enemy->position), enemy->radius / 25 + 0.1f, {1, 1, 0});
    if (application.debug) {
        for (auto &enemy : world.get_enemies()) {
            if (!enemy.is_active)
                continue;
            const auto position = to_world(enemy.position);
            if (application.active_agents && !enemy.frozen)
                runtime.circle(position, 0.18f, {1, 1, 0});
            if (application.vectors)
                draw_debug_vector(runtime, position, to_world(enemy.velocity) * 0.3f, layer_color::vectors);
            if (application.collisions)
                draw_debug_circle(runtime, position, enemy.radius / 25, layer_color::collisions);
            if (application.ai) {
                const std::string_view label = enemy.is_kamikazing()   ? "KAMIKAZE"
                                               : enemy.is_recovering() ? "RECOVER"
                                               : enemy.is_pursueing()  ? "PURSUE"
                                               : enemy.is_attacking()  ? "ATTACK"
                                                                       : "WANDER";
                const auto screen = runtime.camera.world_to_screen(position);
                if (screen.x >= 0 && screen.y >= 0 && screen.x < runtime.camera.viewport.x &&
                    screen.y < runtime.camera.viewport.y) {
                    // Match the projected body size while keeping text and clearance in screen pixels.
                    const float radius = enemy.radius / 25 * runtime.camera.pixels_per_unit;
                    const float width = float(label.size() * 6 - 1);
                    const Vec2 origin = screen + Vec2{-width / 2, -radius - 6 - 7};
                    draw_text(runtime, label, origin, 1, color_of_state(enemy));
                }
            }
        }
        if (application.vectors)
            draw_debug_vector(runtime, to_world(world.get_player().position),
                              to_world(world.get_player().velocity) * 0.3f, layer_color::vectors);
    }
    if (application.debug && application.collisions) {
        draw_debug_circle(runtime, to_world(world.get_player().position), world.get_player().radius / 25,
                          layer_color::collisions);
        for (const auto &bolt : world.get_bolts())
            if (bolt.is_active)
                draw_debug_circle(runtime, to_world(bolt.position), dotio::Bolt::radius / 25, {1, 1, 0});
    }
    if (application.debug)
        for (auto &enemy : world.get_enemies()) {
            if (!enemy.is_active)
                continue;
            const auto &p = application.paths;
            const Vec2 position = to_world(enemy.position);
            const Vec2 heading = normalized(to_world(enemy.velocity));
            // A feeler and, where it meets a wall, the point of contact.
            const auto draw_probe = [&](kinetra::Segment probe) {
                runtime.line(to_world(probe.start), to_world(probe.end), 0.04f, color_of_state(enemy));
                kinetra::Vec2 hit{};
                if (application.world().grid().is_intersecting_any_cell(probe, hit))
                    runtime.circle(to_world(hit), 0.12f, {1, 1, 0});
            };
            if (application.probes) {
                // An enemy that wanders by steering shows the three feelers its steering
                // tests; every other one the original probes: forward and two to the rear.
                if (!enemy.frozen && enemy.is_wandering() &&
                    world.enemy_tuning().wander == dotio::WanderMode::steering)
                    for (const auto &feeler : dotio::wander_feelers(enemy))
                        draw_probe(feeler);
                else
                    for (const float angle : {0.f, 2.35619449f, -2.35619449f})
                        draw_probe({enemy.position, from_world(position + rotated(heading, angle) *
                                                                              (angle == 0 ? 4.f : 1.2f))});
            }
            if (p[5] && enemy.is_recovering()) {
                kinetra::Vec2 hit{};
                if (application.world().grid().is_intersecting_any_cell({enemy.position, enemy.target},
                                                                        hit)) {
                    runtime.line(to_world(enemy.position), to_world(enemy.target), 0.04f,
                                 layer_color::hiding_hits);
                    runtime.circle(to_world(hit), 0.2f, {1, 1, 0});
                }
            }
        }
    const float scale = std::min({2.f, runtime.camera.viewport.x / 270, runtime.camera.viewport.y / 240});
    auto &player = world.get_player();
    const std::array values{player.health / player.max_health, player.mana / player.max_mana,
                            static_cast<float>(player.kills - 1) / player.max_bullet_kills};
    const std::array colors{Color{0.8f, 0, 0}, Color{0, 0.3f, 1}, Color{0.3f, 0.9f, 0}};
    const std::array<std::string_view, 3> labels{"HEALTH", "MANA", "BULLET SPEED"};
    for (std::size_t i = 0; i < values.size(); ++i) {
        const float y = runtime.camera.viewport.y - (62.f - static_cast<float>(i) * 18) * scale;
        panel(runtime, {100 * scale, y}, {92 * scale, 7 * scale}, {0.1f, 0.1f, 0.1f});
        const float width = 184 * scale * std::clamp(values[i], 0.f, 1.f);
        if (width > 0)
            panel(runtime, {8 * scale + width / 2, y}, {width / 2, 7 * scale}, colors[i]);
        draw_text(runtime, labels[i], {12 * scale, y - 3 * scale}, scale, {1, 1, 1});
    }
    if (application.statistics) {
        const auto rates = "FPS " + std::to_string(int(application.frame_rate.rate())) + " UPS " +
                           std::to_string(int(application.update_rate.rate())) + " TPS " +
                           std::to_string(int(application.decision_rate.rate()));
        std::string population;
        if (application.debug) {
            const auto counts = application.agent_counts();
            population =
                "AGENTS " + std::to_string(counts.loaded) + " ACTIVE " + std::to_string(counts.active);
        }
        const auto columns = std::max(rates.size(), population.size());
        const float statistics_scale =
            std::min(scale, runtime.camera.viewport.x / static_cast<float>(columns * 6 + 16));
        // On narrow windows, place the block above the health/mana HUD instead of over its bars.
        const bool overlaps_hud =
            runtime.camera.viewport.x - static_cast<float>(columns * 6 + 8) * statistics_scale < 200 * scale;
        const float baseline = runtime.camera.viewport.y - (overlaps_hud ? 80 : 18) * statistics_scale;
        const auto draw_row = [&](const std::string &text, float y) {
            draw_text(
                runtime, text,
                {runtime.camera.viewport.x - static_cast<float>(text.size() * 6 + 8) * statistics_scale, y},
                statistics_scale, {1, 1, 0});
        };
        if (!population.empty())
            draw_row(population, baseline - 12 * statistics_scale);
        draw_row(rates, baseline);
    }
    if (menu.visible)
        menu.draw(runtime, application.debug_entries());
    kinetra_lab::draw_inspector(runtime, menu, inspector, snapshot ? &*snapshot : nullptr,
                                application.frame_times ? &runtime.frame_times() : nullptr);
    settings.draw(runtime);
    runtime.end();
}

std::string status_text(Application &application) {
    auto &player = application.world().get_player();
    return std::string(application.world().difficulty_settings().name) + " | " +
           std::string(application.paused ? "PAUSED" : "playing") + " | health " +
           std::to_string(int(player.health)) + " | mana " + std::to_string(int(player.mana)) + " | kills " +
           std::to_string(player.kills - 1) + (application.debug ? " | DEBUG" : "") +
           (application.ghost ? " | GHOST" : "");
}
} // namespace dotio
