// Tests for the Dotio presentation: the wall palette without a window, then input
// mapping, culling, statistics, AI labels and the Ghost marker, read back from a real
// OpenGL context. Those context checks share one Runtime, because only one may exist at a
// time, so they form one case. A last case checks that enemy paths pass under the player.
// CTest labels the suite `graphics`.
#include "dotio/entities/physics_adapter.h"
#include "dotio/presentation/presentation.h"
#include "dotio/presentation/session.h"

#include "kinetra_lab/settings.h"
#include <kinetra/runtime/engine.h>
#include <kinetra/runtime/settings_view.h>

#include <doctest/doctest.h>
#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace {
/// The settings toolbar a Dotio session draws over the world, over `application`'s flags.
kinetra::SettingsOverlay settings_for(dotio::Application &application) {
    // The overlay keeps a pointer to the FRAME LIMIT flag, which these checks never read.
    static bool frame_limit = true;
    return kinetra::SettingsOverlay(kinetra_lab::settings_rows(application.statistics, application.debug,
                                                               application.frame_times, frame_limit, {}, {}),
                                    &application.debug);
}
} // namespace

using namespace kinetra;

TEST_CASE("Shift and right-click select a Dotio enemy while the debug menu stays usable") {
    Runtime runtime("Dotio inspector", 1280, 800);
    auto session = dotio::create_session(42, dotio::default_difficulty);
    session->advance(0, {}, runtime.camera);
    auto &application = session->application();
    auto &enemy = application.world().get_enemies()[0];
    enemy.reset();
    enemy.position = application.world().get_player().position + kinetra::Vec2{80, 0};
    Input pick;
    pick.mouse = runtime.camera.world_to_screen(dotio::to_world(enemy.position));
    pick.right_clicked = true;
    session->advance(0, pick, runtime.camera);
    CHECK_FALSE(application.selected_enemy());
    pick.held[std::size_t(Key::shift)] = true;
    session->advance(0, pick, runtime.camera);
    REQUIRE(application.selected_enemy() == &enemy);

    Input debug_menu;
    debug_menu.pressed[std::size_t(Key::shift)] = debug_menu.pressed[std::size_t(Key::control)] = true;
    session->advance(0, debug_menu, runtime.camera);
    Input click;
    click.clicked = true;
    click.mouse =
        Vec2{8, 32} + Vec2{12, DebugMenu::row_top(1) + 8} * DebugMenu::scale(runtime.camera.viewport);
    session->advance(0, click, runtime.camera);
    CHECK(application.paused);
    CHECK(application.selected_enemy() == &enemy);
    click.mouse = {1260, 20};
    session->advance(0, click, runtime.camera);
    CHECK_FALSE(application.selected_enemy());
}

TEST_CASE("Dotio inspector allows world fire and Ghost teleport and consumes close clicks") {
    Runtime runtime("Dotio inspector pointer", 1280, 800);
    auto session = dotio::create_session(42, dotio::default_difficulty);
    auto &camera = runtime.camera;
    session->advance(0, {}, camera);
    auto &application = session->application();
    auto &world = application.world();
    auto &enemy = world.get_enemies()[0];
    auto &player = world.get_player();
    enemy.reset();
    enemy.position = player.position + kinetra::Vec2{80, 0};
    const auto pick = [&] {
        Input input;
        input.right_clicked = true;
        input.held[std::size_t(Key::shift)] = true;
        input.mouse = camera.world_to_screen(dotio::to_world(enemy.position));
        session->advance(0, input, camera);
        REQUIRE(application.selected_enemy() == &enemy);
    };
    const auto click = [&](Vec2 point, double seconds = 0) {
        Input input;
        input.mouse = point;
        input.clicked = true;
        session->advance(seconds, input, camera);
    };
    const Vec2 close{1260, 20};
    pick();
    player.cooldown = 0;
    click({650, 400}, dotio::Application::tick_seconds);
    CHECK(player.cooldown > 0);
    player.cooldown = 0;
    click(close, dotio::Application::tick_seconds);
    CHECK_FALSE(application.selected_enemy());
    CHECK(player.cooldown == 0);

    pick();
    application.debug = application.ghost = true;
    world.get_ghost().position = player.position;
    const auto free_target = [&](auto position) {
        const std::array offsets{kinetra::Vec2{16, 0}, kinetra::Vec2{-16, 0}, kinetra::Vec2{0, 16},
                                 kinetra::Vec2{0, -16}};
        for (auto offset : offsets)
            if (application.teleport_clear(position + offset, player.radius))
                return position + offset;
        FAIL("No nearby free teleport target");
        return position;
    };
    const auto target = free_target(player.position);
    const auto absolute = target + world.world_offset();
    click(camera.world_to_screen(dotio::to_world(target)));
    CHECK(kinetra::length(player.position + world.world_offset() - absolute) < 0.01f);
    REQUIRE(application.selected_enemy());
    const auto after_teleport = player.position + world.world_offset();
    const auto blocked_click_target = free_target(player.position);
    camera.position += dotio::to_world(blocked_click_target) - camera.screen_to_world(close);
    click(close);
    CHECK_FALSE(application.selected_enemy());
    CHECK(player.position + world.world_offset() == after_teleport);
}

namespace {
// Yellow statistics pixels in the right quarter of the frame that the last end() read back,
// and with `rows`, which of its rows hold one.
int statistics_pixels(const Runtime &runtime, std::vector<bool> *rows = nullptr) {
    const auto &frame = runtime.frame();
    if (rows)
        rows->assign(std::size_t(frame.height), false);
    int count = 0;
    for (int y = 0; y < frame.height; ++y)
        for (int x = frame.width - frame.width / 4; x < frame.width; ++x)
            if (const auto pixel = frame.pixel(x, y); pixel[0] > 200 && pixel[1] > 200 && pixel[2] < 80) {
                ++count;
                if (rows)
                    (*rows)[std::size_t(y)] = true;
            }
    return count;
}
// Player-colored pixels in the 32 x 32 square at the center of the last read-back frame.
int ghost_marker_pixels(const Runtime &runtime) {
    const auto &frame = runtime.frame();
    int count = 0;
    for (int y = frame.height / 2 - 16; y < frame.height / 2 + 16; ++y)
        for (int x = frame.width / 2 - 16; x < frame.width / 2 + 16; ++x) {
            const auto pixel = frame.pixel(x, y);
            count += pixel[0] > 80 && pixel[0] < 125 && pixel[1] < 20 && pixel[2] > 240;
        }
    return count;
}
// Toggles AI labels on an enemy at the camera center and checks the changed pixels: a
// centered label of fixed screen size with fixed clearance above the body, at every zoom.
void ai_label_placement(Runtime &runtime, dotio::Application &application) {
    auto &enemy = application.world().get_enemies()[0];
    enemy.is_active = true;
    enemy.position = dotio::from_world(runtime.camera.position);
    application.debug = true;
    for (const float zoom : {2.5f, 25.f, 60.f}) {
        runtime.camera.pixels_per_unit = zoom;
        CAPTURE(zoom);
        for (const bool kamikaze : {false, true}) {
            CAPTURE(kamikaze);
            if (kamikaze)
                enemy.set_state(enemy.get_kamikaze_state());
            else
                enemy.set_state(enemy.get_wander_state());
            application.ai = false;
            dotio::render(runtime, application, settings_for(application), kinetra::DebugMenu{});
            const auto without_label = runtime.frame().rgba;
            application.ai = true;
            dotio::render(runtime, application, settings_for(application), kinetra::DebugMenu{});
            const auto with_label = runtime.frame().rgba;
            const int width = int(runtime.camera.viewport.x), height = int(runtime.camera.viewport.y);
            int left = width, right = -1, top = height, bottom = -1;
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    const int offset = (y * width + x) * 4;
                    if (with_label[offset] != without_label[offset] ||
                        with_label[offset + 1] != without_label[offset + 1] ||
                        with_label[offset + 2] != without_label[offset + 2]) {
                        left = std::min(left, x);
                        right = std::max(right, x);
                        top = std::min(top, y);
                        bottom = std::max(bottom, y);
                    }
                }
            CHECK_MESSAGE(
                (right >= left && std::abs(float(left + right + 1) / 2.f - float(width) / 2.f) <= 1),
                "AI label is missing or not centered above the agent");
            CHECK_MESSAGE((bottom - top == 6 && right - left == (kamikaze ? 46 : 34)),
                          "AI label size changed with camera zoom");
            const float body_top = float(height) / 2.f - enemy.radius / 25 * zoom;
            CHECK_MESSAGE(std::abs(body_top - float(bottom + 1) - 6) <= 1,
                          "AI label lost its screen-pixel clearance above the projected body");
        }
    }
    enemy.is_active = false;
    application.debug = application.ai = false;
    runtime.camera.pixels_per_unit = 25;
}
int statistics_bands(const Runtime &runtime) {
    std::vector<bool> rows;
    statistics_pixels(runtime, &rows);
    int bands = 0;
    bool previous = false;
    for (const bool row : rows) {
        bands += row && !previous;
        previous = row;
    }
    return bands;
}
} // namespace

TEST_CASE("drawing interpolates the player and leaves the simulation and camera untouched") {
    Runtime reopened("Dotio interpolation", 320, 240);
    dotio::Application application(42);
    dotio::Commands commands;
    commands.direction = {1, 0};
    application.advance(0, commands, reopened.camera);
    // Five whole 60 Hz ticks and half of the next one; the clock runs at most eight a frame.
    application.advance(5.5 / 60, commands, reopened.camera);
    CHECK(application.interpolation() == doctest::Approx(0.5).epsilon(0.01));
    const auto objects = application.moving_objects().size();
    REQUIRE((application.before_tick().size() == objects && application.after_tick().size() == objects));
    auto &player = application.world().get_player();
    const auto simulated = player.position;
    const auto offset = application.world().world_offset();
    REQUIRE(application.after_tick()[0]);
    CHECK_MESSAGE(*application.after_tick()[0] == simulated + offset,
                  "The snapshot after the last tick must match the simulated player");
    CHECK_MESSAGE(application.before_tick()[0]->x != application.after_tick()[0]->x,
                  "The moving player did not move during the last tick");
    {
        const dotio::PresentedPositions presented(application);
        const auto &before = *application.before_tick()[0], &after = *application.after_tick()[0];
        const float blend = float(application.interpolation());
        CHECK_MESSAGE(
            (std::abs(player.position.x + offset.x - (before.x + (after.x - before.x) * blend)) < 1e-3f &&
             std::abs(player.position.y + offset.y - (before.y + (after.y - before.y) * blend)) < 1e-3f),
            "While drawing, the player must sit between its last two ticks");
    }
    CHECK_MESSAGE(player.position == simulated, "PresentedPositions did not restore the simulated position");
    const auto camera = reopened.camera.position;
    dotio::render(reopened, application, settings_for(application), kinetra::DebugMenu{});
    CHECK_MESSAGE(player.position == simulated, "Drawing left the interpolated position in the simulation");
    CHECK_MESSAGE(reopened.camera.position == camera, "Drawing left the camera on the interpolated player");
}

TEST_CASE("wall colors vary across the map without seams or low contrast") {
    const auto first_color = dotio::wall_color(-120, 30);
    const auto distant_color = dotio::wall_color(120, -30);
    CHECK_MESSAGE(std::abs(first_color.r - distant_color.r) + std::abs(first_color.g - distant_color.g) >
                      0.05f,
                  "Walls lost their spatial color variation");
    for (int row = -200; row <= 200; ++row) {
        const auto color = dotio::wall_color(row, row * 2);
        const auto next = dotio::wall_color(row + 1, row * 2 + 1);
        CHECK_MESSAGE((color.r >= 0.19f && color.g >= 0.21f && color.b >= 0.26f && color.r <= 0.45f &&
                       color.g <= 0.51f && color.b <= 0.58f && color.a == 1),
                      "Wall palette lost contrast against the black background");
        CHECK_MESSAGE((std::abs(color.r - next.r) < 0.02f && std::abs(color.g - next.g) < 0.02f &&
                       std::abs(color.b - next.b) < 0.02f),
                      "Wall gradient has a color seam");
    }
}

TEST_CASE("input culling statistics AI labels and the Ghost marker render as specified") {
    Runtime reopened("Dotio presentation", 320, 240);
    reopened.read_back(true);
    Input controls;
    controls.pressed[std::size_t(Key::space)] = true;
    controls.held[std::size_t(Key::shift)] = true;
    controls.clicked = controls.mouse_down = true;
    controls.mouse = {150, 200};
    controls.wheel = 2;
    controls.held[std::size_t(Key::right)] = true;
    controls.held[std::size_t(Key::w)] = true;
    const auto mapped = dotio::read_commands(controls, reopened.camera);
    CHECK_MESSAGE((mapped.teleport && mapped.shoot && mapped.zoom_steps == 2),
                  "Dotio teleport/shoot/wheel mapping failed");
    CHECK_MESSAGE((mapped.direction.x == 1 && mapped.direction.y == 1 && mapped.camera_pan.x == 1),
                  "Dotio WASD and arrows must share movement intent");
    dotio::Application application(42);
    application.advance(0, {}, reopened.camera);
    dotio::render(reopened, application, settings_for(application), kinetra::DebugMenu{});
    CHECK_MESSAGE(statistics_pixels(reopened) == 0, "Disabled statistics rendered outside debug mode");
    // Submitted geometry must stay independent of the offscreen resident population.
    const auto geometry_bytes = [&]() {
        dotio::render(reopened, application, settings_for(application), kinetra::DebugMenu{});
        GLint bytes = 0;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bytes);
        return bytes;
    };
    const auto baseline_bytes = geometry_bytes();
    auto &enemy = application.world().get_enemies()[0];
    enemy.is_active = true;
    enemy.position = dotio::from_world(reopened.camera.position + Vec2{1000, 1000});
    CHECK_MESSAGE(geometry_bytes() == baseline_bytes, "Offscreen enemy still submits circle geometry");
    const float half_width = reopened.camera.viewport.x / (2 * reopened.camera.pixels_per_unit);
    enemy.position = dotio::from_world(reopened.camera.position + Vec2{half_width + 0.2f, 0});
    CHECK_MESSAGE(geometry_bytes() > baseline_bytes, "Culling removed an edge-crossing enemy circle");
    enemy.is_active = false;
    auto &arrow = application.world().get_arrows()[0];
    arrow.is_active = true;
    arrow.velocity = {1, 0};
    arrow.position = dotio::from_world(reopened.camera.position + Vec2{1000, 1000});
    CHECK_MESSAGE(geometry_bytes() == baseline_bytes, "Offscreen arrow still submits segment geometry");
    arrow.position = dotio::from_world(reopened.camera.position + Vec2{half_width + 0.2f, 0});
    CHECK_MESSAGE(geometry_bytes() > baseline_bytes, "Culling removed an edge-crossing arrow segment");
    arrow.is_active = false;

    ai_label_placement(reopened, application);

    application.statistics = true;
    dotio::render(reopened, application, settings_for(application), kinetra::DebugMenu{});
    CHECK_MESSAGE((!application.debug && statistics_pixels(reopened) > 0),
                  "Enabled statistics are hidden outside debug mode");
    CHECK_MESSAGE(statistics_bands(reopened) == 1, "Normal statistics is not a single row");
    application.debug = true;
    dotio::render(reopened, application, settings_for(application), kinetra::DebugMenu{});
    CHECK_MESSAGE(statistics_bands(reopened) == 2,
                  "Agent counts are not on a separate row above timing statistics");
    application.debug = false;
    application.statistics = false;
    dotio::render(reopened, application, settings_for(application), kinetra::DebugMenu{});
    CHECK_MESSAGE(statistics_pixels(reopened) == 0, "Statistics toggle did not hide the overlay");
    application.debug = application.vectors = application.collisions = application.ai = true;
    application.probes = application.statistics = true;
    application.paths.fill(true);
    dotio::render(reopened, application, settings_for(application), kinetra::DebugMenu{});
    CHECK_MESSAGE(glGetError() == GL_NO_ERROR, "Dotio debug rendering failed");
    application.debug = application.ghost = true;
    application.world().get_ghost().position = application.world().get_player().position;
    application.world().get_ghost().position.x += 1000;
    reopened.camera.position = dotio::to_world(application.world().get_ghost().position);
    dotio::render(reopened, application, settings_for(application), kinetra::DebugMenu{});
    CHECK_MESSAGE(ghost_marker_pixels(reopened) == 0, "Ghost camera still draws a purple player marker");
}

TEST_CASE("an enemy's path passes under the player it crosses") {
    Runtime runtime("Dotio paths under bodies", 640, 480);
    runtime.read_back(true);
    dotio::Application application(42);
    application.debug = true;
    application.paths.fill(true);
    application.world().enemy_tuning().wander = dotio::WanderMode::path;
    // Zoomed in, so a path, 0.05 world units wide, covers whole pixels; the application keeps
    // the zoom within its limits.
    runtime.camera.pixels_per_unit = 100;
    auto &world = application.world();
    const auto player_color = [](std::array<std::uint8_t, 4> pixel) {
        // The player, {0.4, 0, 1}.
        return std::abs(pixel[0] - 102) < 30 && pixel[1] < 30 && pixel[2] > 225;
    };
    // A point of an enemy's path inside the player, both as drawn this frame. The player has
    // no path of its own.
    std::optional<Vec2> crossing;
    Vec2 shift;
    for (int frame = 0; frame < 3000 && !crossing; ++frame) {
        application.advance(1.0 / 60, {}, runtime.camera);
        const auto &player = world.get_player();
        const auto simulated = dotio::to_world(player.position);
        const dotio::PresentedPositions presented(application);
        // The renderer moves the camera onto the drawn player for the frame.
        shift = dotio::to_world(player.position) - simulated;
        for (auto &enemy : world.get_enemies()) {
            if (!enemy.is_active || enemy.frozen || enemy.path.remaining().empty())
                continue;
            Vec2 from = enemy.position;
            for (const auto waypoint : enemy.path.remaining()) {
                const auto segment = waypoint - from;
                const float squared = dot(segment, segment);
                const auto point =
                    from + segment * (squared > 0 ? std::clamp(dot(player.position - from, segment) / squared,
                                                               0.f, 1.f)
                                                  : 0.f);
                if (length(point - player.position) < 0.6f * player.radius) {
                    crossing = dotio::to_world(point);
                    break;
                }
                from = waypoint;
            }
            if (crossing)
                break;
        }
    }
    REQUIRE_MESSAGE(crossing, "No enemy path crossed the player");
    dotio::render(runtime, application, settings_for(application), kinetra::DebugMenu{});
    auto camera = runtime.camera;
    camera.position += shift;
    const auto at = camera.world_to_screen(*crossing);
    CHECK_MESSAGE(player_color(runtime.frame().pixel(int(std::lround(at.x)), int(std::lround(at.y)))),
                  "An enemy's path covers the player");
}
