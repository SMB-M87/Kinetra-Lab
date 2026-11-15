#include "../include/presentation.h"
#include <kinetra/help_view.h>
#include "../include/physics_adapter.h"

namespace dotio {
using namespace kinetra;
Color wall_color(int absolute_row, int absolute_column) {
    // Echo the original row/column gradient without dark corners or hard color resets.
    return {0.32f + 0.12f * std::sin(absolute_row * 0.055f),
            0.36f + 0.14f * std::sin(absolute_column * 0.055f + 2.094f),
            float(0.42 + 0.15 * std::sin((double(absolute_row) + absolute_column) * 0.035 + 4.189))};
}

Commands read_commands(const Input &input, const Camera2D &camera) {
    Commands commands;
    commands.direction = {float(input.down(Key::d) || input.down(Key::right)) -
                              float(input.down(Key::a) || input.down(Key::left)),
                          float(input.down(Key::w) || input.down(Key::up)) -
                              float(input.down(Key::s) || input.down(Key::down))};
    commands.camera_pan = commands.direction;
    commands.aim = camera.screen_to_world(input.mouse);
    commands.shoot = input.mouse_down || input.clicked;
    commands.pause = input.hit(Key::space);

    commands.help = input.hit(Key::h);

    commands.teleport = input.clicked;
    commands.teleport_target = commands.aim;

    commands.zoom_steps = input.wheel;

    return commands;
}

namespace {
class Renderer final : public gamef::Renderer {
    Runtime &runtime_;
    Vec2 half_view_;

    // Reject whole primitive bounds before circle tessellation and vertex uploads.
    // Keep edge-crossing circles and segments even when their centers are offscreen.
    bool visible(Vec2 center, Vec2 half) const {
        const auto delta = center - runtime_.camera.position;
        return std::abs(delta.x) <= half_view_.x + half.x && std::abs(delta.y) <= half_view_.y + half.y;
    }

  public:
    explicit Renderer(Runtime &runtime)
        : runtime_(runtime), half_view_(runtime.camera.viewport / (2 * runtime.camera.pixels_per_unit)) {}
    void DrawCircle(gmath::Position center, float radius, gmath::RGBA color) override {
        const auto position = to_world(center);
        const float world_radius = radius / 25;
        if (visible(position, {world_radius, world_radius}))
            runtime_.circle(position, world_radius, {color.R, color.G, color.B, color.A});
    }
    void DrawSegment(gmath::Segment segment, gmath::RGBA color) override {
        const auto start = to_world(segment.Start), end = to_world(segment.End);
        const Vec2 half{std::abs(end.x - start.x) / 2 + 0.03f, std::abs(end.y - start.y) / 2 + 0.03f};
        if (visible((start + end) / 2, half))
            runtime_.line(start, end, 0.06f, {color.R, color.G, color.B, color.A});
    }
};
Color state_color(logic::Enemy &enemy) {
    return enemy.IsKamikazing()   ? Color{1, 0.3f, 0.3f}
           : enemy.IsRecovering() ? Color{0, 1, 0}
           : enemy.IsPursueing()  ? Color{1, 0.65f, 0}
           : enemy.IsAttacking()  ? Color{1, 0, 0}
                                  : Color{0, 0.5f, 1};
}
void panel(Runtime &runtime, Vec2 center, Vec2 half, Color color) {
    runtime.box(runtime.camera.screen_to_world(center), half / runtime.camera.pixels_per_unit, 0, color);
}
} // namespace

void render(Runtime &runtime, Application &application) {
    auto &world = application.world();
    runtime.begin({0, 0, 0});
    Renderer renderer(runtime);
    // Cull generated arena geometry against the active camera.
    const auto top_left = from_world(runtime.camera.screen_to_world({0, 0}));
    const auto bottom_right = from_world(runtime.camera.screen_to_world(runtime.camera.viewport));
    const int first_row = int(std::floor(top_left.Y / 100)) - 1;
    const int last_row = int(std::floor(bottom_right.Y / 100)) + 1;
    const int first_col = int(std::floor(top_left.X / 100)) - 1;
    const int last_col = int(std::floor(bottom_right.X / 100)) + 1;
    const auto offset = world.WorldOffset();
    const int row_offset = int(std::lround(offset.Y / logic::Grid::CellSize));
    const int column_offset = int(std::lround(offset.X / logic::Grid::CellSize));
    for (int row = first_row; row <= last_row; ++row)
        for (int column = first_col; column <= last_col; ++column) {
            const auto center = to_world({column * 100.f + 50, row * 100.f + 50});
            if (!world.TerrainCell(row, column))
                continue;
            runtime.box(center, {2, 2}, 0, wall_color(row + row_offset, column + column_offset));
            if (application.debug && application.collisions)
                draw_debug_box(runtime, center, {2, 2}, 0, {0.6f, 0.8f, 1});
        }
    for (const auto &prop : world.GetObjects()) {
        const auto center = to_world(legacy_vector(prop.body.position));
        renderer.DrawCircle(legacy_vector(prop.body.position), prop.body.radius, {0.65f, 0.45f, 0.2f, 1});
        if (application.debug && application.vectors)
            draw_debug_vector(runtime, center, to_world(legacy_vector(prop.body.velocity)), {1, 0.8f, 0.2f});
        if (application.debug && application.collisions)
            draw_debug_circle(runtime, center, prop.body.radius / 25, {1, 0.8f, 0.2f});
    }
    world.Draw(renderer);
    if (application.debug) {
        for (auto &enemy : world.GetEnemies()) {
            if (!enemy.IsActive)
                continue;
            const auto position = to_world(enemy.Position);
            if (application.active_agents && !enemy.Frozen)
                runtime.circle(position, 0.18f, {1, 1, 0});
            if (application.vectors)
                draw_debug_vector(runtime, position, to_world(enemy.Velocity) * 0.3f, {0.8f, 1, 0.3f});
            if (application.collisions)
                draw_debug_circle(runtime, position, enemy.Radius / 25, {0.6f, 0.8f, 1});
            if (application.ai) {
                const char *label = enemy.IsKamikazing()   ? "KAMIKAZE"
                                    : enemy.IsRecovering() ? "RECOVER"
                                    : enemy.IsPursueing()  ? "PURSUE"
                                    : enemy.IsAttacking()  ? "ATTACK"
                                                           : "WANDER";
                const auto screen = runtime.camera.world_to_screen(position);
                if (screen.x >= 0 && screen.y >= 0 && screen.x < runtime.camera.viewport.x &&
                    screen.y < runtime.camera.viewport.y)
                    draw_text(runtime, label, screen + Vec2{8, -10}, 1, state_color(enemy));
            }
        }
        if (application.vectors)
            draw_debug_vector(runtime, to_world(world.GetPlayer().Position),
                              to_world(world.GetPlayer().Velocity) * 0.3f, {0.8f, 1, 0.3f});
    }
    if (application.debug && application.collisions) {
        draw_debug_circle(runtime, to_world(world.GetPlayer().Position), world.GetPlayer().Radius / 25,
                          {0.6f, 0.8f, 1});
        for (const auto &bolt : world.GetBolts())
            if (bolt.IsActive)
                draw_debug_circle(runtime, to_world(bolt.Position), logic::Bolt::Radius / 25, {1, 1, 0});
    }
    if (application.debug)
        for (auto &enemy : world.GetEnemies()) {
            if (!enemy.IsActive)
                continue;
            const auto &p = application.paths;
            if (p[0] && !enemy.Frozen &&
                ((p[1] && enemy.IsWandering()) || (p[2] && enemy.IsPursueing()) ||
                 (p[3] && enemy.IsKamikazing()) || (p[4] && enemy.IsRecovering()))) {
                auto start = to_world(enemy.Position);
                for (int i = enemy.Path.size() - 1; i >= 0; --i) {
                    const auto cell = enemy.Path.get(i);
                    const auto end = to_world({cell.Column * 100.f + 50, cell.Row * 100.f + 50});
                    runtime.line(start, end, 0.05f, state_color(enemy));
                    start = end;
                }
            }
            // Original debug probes: forward and two rear feelers, including wall intersections.
            const Vec2 position = to_world(enemy.Position);
            const Vec2 heading = normalized(to_world(enemy.Velocity));
            if (application.probes)
                for (const float angle : {0.f, 2.35619449f, -2.35619449f}) {
                    const Vec2 end = position + rotated(heading, angle) * (angle == 0 ? 4.f : 1.2f);
                    runtime.line(position, end, 0.04f, state_color(enemy));
                    gmath::Point2 hit{};
                    if (logic::Grid::IsIntersectingAnyCell({enemy.Position, from_world(end)},
                                                           &logic::Grid::Array[0][0], hit))
                        runtime.circle(to_world(hit), 0.12f, {1, 1, 0});
                }
            if (p[5] && enemy.IsRecovering()) {
                gmath::Point2 hit{};
                if (logic::Grid::IsIntersectingAnyCell({enemy.Position, enemy.Target},
                                                       &logic::Grid::Array[0][0], hit)) {
                    runtime.line(to_world(enemy.Position), to_world(enemy.Target), 0.04f, {0, 1, 0});
                    runtime.circle(to_world(hit), 0.2f, {1, 1, 0});
                }
            }
        }
    constexpr std::array<std::string_view, 9> help{"DOTIO CONTROLS   H: HIDE HELP",
                                                   "WASD / ARROWS  MOVE PLAYER",
                                                   "HOLD CLICK     SHOOT",
                                                   "MOUSE WHEEL    ZOOM",
                                                   "SPACE          PAUSE",
                                                   "BACKQUOTE      DEBUG MENU",
                                                   "GHOST: CLICK   TELEPORT PLAYER",
                                                   "M              START MENU",
                                                   "ESC            EXIT"};
    const float scale = std::min({2.f, runtime.camera.viewport.x / 270, runtime.camera.viewport.y / 240});
    HelpStyle help_style;
    help_style.reference_view = {270, 240};
    help_style.width = help_style.collapsed_width = 260;
    help_style.expanded_height = 156;
    help_style.origin = {2, 2};
    help_style.text_origin = {8, 8};
    help_style.collapsed = "H: CONTROLS   M: MENU";
    help_style.background = {0.02f, 0.02f, 0.04f};
    help_style.heading = help_style.text = {0.85f, 0.85f, 1};
    draw_help(runtime, help, application.help, help_style);
    auto &player = world.GetPlayer();
    const std::array values{player.Health / player.MaxHealth, player.Mana / player.MaxMana,
                            (player.Kills - 1) / player.MaxBulletKills};
    const std::array colors{Color{0.8f, 0, 0}, Color{0, 0.3f, 1}, Color{0.3f, 0.9f, 0}};
    const std::array<std::string_view, 3> labels{"HEALTH", "MANA", "BULLET SPEED"};
    for (std::size_t i = 0; i < values.size(); ++i) {
        const float y = runtime.camera.viewport.y - (62 - i * 18) * scale;
        panel(runtime, {100 * scale, y}, {92 * scale, 7 * scale}, {0.1f, 0.1f, 0.1f});
        const float width = 184 * scale * std::clamp(values[i], 0.f, 1.f);
        if (width > 0)
            panel(runtime, {8 * scale + width / 2, y}, {width / 2, 7 * scale}, colors[i]);
        draw_text(runtime, labels[i], {12 * scale, y - 3 * scale}, scale, {1, 1, 1});
    }
    if (application.statistics) {
        const auto rates = "FPS " + std::to_string(int(application.fps)) + " UPS " +
                           std::to_string(int(application.ups)) + " TPS " +
                           std::to_string(int(application.tps));
        std::string population;
        if (application.debug) {
            const auto counts = application.agent_counts();
            population =
                "AGENTS " + std::to_string(counts.loaded) + " ACTIVE " + std::to_string(counts.active);
        }
        const auto columns = std::max(rates.size(), population.size());
        const float statistics_scale = std::min(scale, runtime.camera.viewport.x / (columns * 6 + 16));
        // On narrow windows, place the block above the health/mana HUD instead of over its bars.
        const bool overlaps_hud =
            runtime.camera.viewport.x - (columns * 6 + 8) * statistics_scale < 200 * scale;
        const float baseline = runtime.camera.viewport.y - (overlaps_hud ? 80 : 18) * statistics_scale;
        const auto draw_row = [&](const std::string &text, float y) {
            draw_text(runtime, text,
                      {runtime.camera.viewport.x - (text.size() * 6 + 8) * statistics_scale, y},
                      statistics_scale, {1, 1, 0});
        };
        if (!population.empty())
            draw_row(population, baseline - 12 * statistics_scale);
        draw_row(rates, baseline);
    }
    const auto options = application.debug_options();
    application.menu.draw(runtime, options);
    runtime.end();
}

std::string window_title(Application &application) {
    auto &player = application.world().GetPlayer();
    return "Kinetra | Dotio | " + std::string(application.world().difficulty_settings().name) + " | " +
           std::string(application.paused ? "PAUSED" : "playing") + " | health " +
           std::to_string(int(player.Health)) + " | mana " + std::to_string(int(player.Mana)) + " | kills " +
           std::to_string(player.Kills - 1) + (application.debug ? " | DEBUG" : "") +
           (application.ghost ? " | GHOST" : "") + " | H controls | M menu";
}
} // namespace dotio
