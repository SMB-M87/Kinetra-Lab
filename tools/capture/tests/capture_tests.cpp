// Window-free tests for kinetra_capture, whose headers are in tools/capture/include/capture/: the
// PNG writer (png.h), the scene list and the menu rows the scripts click (scenes.h), the showcase
// cycle measurement (showcase.h), the busiest stretch of the captured i4sim floor (busy_stretch.h),
// the scripted Dotio player (dotio_player.h), and the tour's frame drawing (canvas.h), pointer
// motion (pointer_path.h) and row lookup (tour.h). No Runtime or session is created.
#include "capture/dotio_player.h"
#include "capture/png.h"
#include "capture/scenes/busy_stretch.h"
#include "capture/scenes/scenes.h"
#include "capture/scenes/showcase.h"
#include "capture/stage.h"
#include "capture/tour/canvas.h"
#include "capture/tour/pointer_path.h"
#include "capture/tour/tour.h"

#include "demo/debug_panel.h"
#include "demo/presentation/session.h"
#include "i4sim/controller.h"
#include "i4sim/simulation/simulation.h"

#include <zlib.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
std::filesystem::path temporary_file(const std::string &name) {
    return std::filesystem::temp_directory_path() /
           (name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            ".png");
}

std::vector<std::uint8_t> read_file(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

std::uint32_t read_u32(const std::vector<std::uint8_t> &bytes, std::size_t at) {
    return std::uint32_t(bytes.at(at)) << 24 | std::uint32_t(bytes.at(at + 1)) << 16 |
           std::uint32_t(bytes.at(at + 2)) << 8 | std::uint32_t(bytes.at(at + 3));
}

struct Chunk {
    std::string type;
    std::vector<std::uint8_t> data;
};

/// The chunks after the signature; checks every CRC.
std::vector<Chunk> chunks(const std::vector<std::uint8_t> &file) {
    std::vector<Chunk> result;
    std::size_t at = 8;
    while (at < file.size()) {
        const auto length = read_u32(file, at);
        Chunk chunk{std::string(file.begin() + long(at + 4), file.begin() + long(at + 8)),
                    {file.begin() + long(at + 8), file.begin() + long(at + 8 + length)}};
        const auto crc = crc32(0, file.data() + at + 4, uInt(length + 4));
        CHECK_MESSAGE(read_u32(file, at + 8 + length) == crc, "bad CRC in " << chunk.type);
        result.push_back(std::move(chunk));
        at += 12 + length;
    }
    return result;
}

/// The RGB pixels of a file written by write_png: inflates IDAT and undoes the none and up
/// filters, the only two it uses.
std::vector<std::uint8_t> decode(const std::vector<Chunk> &parts, std::uint32_t width, std::uint32_t height) {
    const std::size_t stride = std::size_t(width) * 3;
    std::vector<std::uint8_t> rows((stride + 1) * height);
    uLongf size = uLongf(rows.size());
    REQUIRE(parts.at(1).type == "IDAT");
    REQUIRE(uncompress(rows.data(), &size, parts[1].data.data(), uLong(parts[1].data.size())) == Z_OK);
    REQUIRE(size == rows.size());
    std::vector<std::uint8_t> pixels(stride * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto filter = rows[y * (stride + 1)];
        REQUIRE((filter == 0 || filter == 2));
        for (std::size_t i = 0; i < stride; ++i) {
            const auto value = rows[y * (stride + 1) + 1 + i];
            pixels[y * stride + i] =
                filter == 0 ? value : std::uint8_t(value + (y > 0 ? pixels[(y - 1) * stride + i] : 0));
        }
    }
    return pixels;
}
} // namespace

TEST_CASE("write_png writes a valid RGB file that decodes to its pixels") {
    constexpr std::uint32_t width = 7, height = 5;
    std::vector<std::uint8_t> pixels(width * height * 3);
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = std::uint8_t(i * 37 % 251);
    // Two equal rows, which the writer stores as differences.
    std::copy_n(pixels.begin(), width * 3, pixels.begin() + width * 3);
    const auto path = temporary_file("kinetra-capture-test");
    capture::write_png(path, width, height, pixels);
    const auto file = read_file(path);
    std::filesystem::remove(path);
    REQUIRE(file.size() > 8);
    CHECK(std::vector<std::uint8_t>(file.begin(), file.begin() + 8) ==
          std::vector<std::uint8_t>{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'});
    const auto parts = chunks(file);
    REQUIRE(parts.size() == 3);
    CHECK(parts[0].type == "IHDR");
    CHECK(parts[2].type == "IEND");
    CHECK(read_u32(parts[0].data, 0) == width);
    CHECK(read_u32(parts[0].data, 4) == height);
    CHECK(parts[0].data.at(8) == 8);
    CHECK_MESSAGE(parts[0].data.at(9) == 2, "color type RGB");
    CHECK(decode(parts, width, height) == pixels);
}

TEST_CASE("write_png rejects empty images, wrong sizes and unwritable paths") {
    const std::vector<std::uint8_t> pixel{1, 2, 3};
    const auto path = temporary_file("kinetra-capture-invalid");
    CHECK_THROWS_AS(capture::write_png(path, 0, 1, {}), std::invalid_argument);
    CHECK_THROWS_AS(capture::write_png(path, 2, 1, pixel), std::invalid_argument);
    CHECK_THROWS_AS(
        capture::write_png(std::filesystem::path("/nonexistent-directory") / "x.png", 1, 1, pixel),
        std::runtime_error);
    CHECK_FALSE(std::filesystem::exists(path));
}

/// The text of a file in the Lab source tree.
static std::string source_text(const std::filesystem::path &relative) {
    std::ifstream in(std::filesystem::path(KINETRA_LAB_SOURCE_DIR) / relative);
    return {std::istreambuf_iterator<char>(in), {}};
}

TEST_CASE("every scene has its file in docs/media and the media guide, and every clip is in the gallery") {
    // The media guide lists every file; the README gallery shows the clips and leaves the full-size
    // application screenshots to the guide.
    const auto readme = source_text("README.md"), guide = source_text("docs/media/README.md");
    for (const auto &scene : capture::scenes()) {
        const bool clip = scene.kind == capture::SceneKind::clip;
        const std::string file = std::string(scene.name) + (clip ? ".gif" : ".png");
        CAPTURE(file);
        CHECK(std::filesystem::exists(std::filesystem::path(KINETRA_LAB_SOURCE_DIR) / "docs/media" / file));
        CHECK(guide.find("(" + file + ")") != std::string::npos);
        if (clip)
            CHECK(readme.find("docs/media/" + file) != std::string::npos);
    }
}

TEST_CASE("the scenes are the expected list, each once") {
    std::set<std::string> names;
    for (const auto &scene : capture::scenes()) {
        CHECK_MESSAGE(names.insert(std::string(scene.name)).second, scene.name << " appears twice");
        CHECK(scene.script != nullptr);
        CHECK((scene.width > 0 && scene.height > 0));
        CHECK((scene.kind == capture::SceneKind::clip) == scene.name.ends_with("-clip"));
    }
    CHECK(names == std::set<std::string>{"launcher",
                                         "launcher-difficulty",
                                         "launcher-loadscreen",
                                         "demo",
                                         "demo-debug",
                                         "demo-clip",
                                         "demo-debug-clip",
                                         "dotio",
                                         "dotio-debug",
                                         "dotio-settings",
                                         "dotio-clip",
                                         "dotio-debug-clip",
                                         "i4sim",
                                         "i4sim-debug",
                                         "i4sim-clip",
                                         "i4sim-debug-clip",
                                         "i4sim-showcase-junction-crossing-clip",
                                         "i4sim-showcase-corridor-passing-clip",
                                         "i4sim-showcase-corridor-recovery-clip",
                                         "i4sim-showcase-predictive-passing-clip",
                                         "i4sim-showcase-h-crossing-clip"});
}

TEST_CASE("select_scenes picks named scenes in order, all without names, and rejects unknown names") {
    CHECK(capture::select_scenes({}).size() == capture::scenes().size());
    const std::vector<std::string> names{"i4sim", "launcher"};
    const auto selected = capture::select_scenes(names);
    REQUIRE(selected.size() == 2);
    CHECK(selected[0]->name == "i4sim");
    CHECK(selected[1]->name == "launcher");
    const std::vector<std::string> unknown{"demo", "screenshot"};
    CHECK_THROWS_AS(capture::select_scenes(unknown), std::invalid_argument);
}

/// A showcase blueprint in the Lab source tree.
static std::filesystem::path showcase_file(const std::string &name) {
    return std::filesystem::path(KINETRA_LAB_SOURCE_DIR) / "labs/i4sim/blueprints/showcase" /
           (name + ".json");
}

TEST_CASE("a showcase cycle holds the scene's pauses and is the same on every run") {
    // The pause in which the movers stand before they turn or the scene restarts, in steps.
    const auto pause = static_cast<int>(double(i4sim::shuttle_pause_ticks) /
                                        capture::i4sim_updates_per_second / capture::Stage::step_seconds);
    // Shuttles stand twice per cycle, at the goal and back at the start; corridor recovery
    // stands once, before it restarts.
    const std::pair<const char *, int> scenes[]{{"junction-crossing", 2},
                                                {"corridor-passing", 2},
                                                {"corridor-recovery", 1},
                                                {"predictive-passing", 2},
                                                {"h-crossing", 2}};
    for (const auto &[name, pauses] : scenes) {
        CAPTURE(name);
        const int steps = capture::showcase_cycle_steps(showcase_file(name));
        CHECK(steps > pauses * pause);
        CHECK(capture::showcase_cycle_steps(showcase_file(name)) == steps);
    }
}

TEST_CASE("showcase_cycle_steps rejects a blueprint without a repeating scenario and a missing file") {
    CHECK_THROWS_AS(capture::showcase_cycle_steps(std::filesystem::path(KINETRA_LAB_SOURCE_DIR) /
                                                  "labs/i4sim/blueprints/3840x2400.json"),
                    std::invalid_argument);
    CHECK_THROWS_AS(capture::showcase_cycle_steps(showcase_file("no-such-scene")), std::runtime_error);
}

/// The captured i4sim blueprint in the Lab source tree.
static std::filesystem::path captured_floor() {
    return std::filesystem::path(KINETRA_LAB_SOURCE_DIR) / "labs/i4sim/blueprints" /
           capture::i4sim_capture_blueprint;
}

TEST_CASE("the i4sim menu clicks find their rows and choose the Reciprocal profile with plain A*") {
    i4sim::FactoryController controller(capture::capture_seed, false, capture::i4sim_updates_per_second);
    REQUIRE(controller.load(captured_floor()));
    // The session shows the controller's entries as menu rows one to one, so a click on a
    // row runs the action of the entry with that index.
    REQUIRE_NOTHROW(capture::apply_menu_clicks(controller, capture::i4sim_motion_clicks));
    const auto &motion = controller.simulation().motion_settings();
    CHECK(motion.profile == i4sim::SteeringProfile::reciprocal);
    CHECK(motion.path_planning);
    CHECK_FALSE(motion.heatmap);
    // The debug screenshot clicks LAYERS in the menu these clicks leave behind.
    const capture::MenuClick layers[]{capture::i4sim_layers_click};
    CHECK_NOTHROW(capture::apply_menu_clicks(controller, layers));
}

TEST_CASE("apply_menu_clicks rejects a row with another label and a row that does not exist") {
    i4sim::FactoryController controller(capture::capture_seed, false, capture::i4sim_updates_per_second);
    REQUIRE(controller.load(captured_floor()));
    const auto before = controller.simulation().motion_settings().profile;
    const capture::MenuClick renamed[]{{6, "MOTION"}, {3, "STEERING"}};
    CHECK_THROWS_AS(capture::apply_menu_clicks(controller, renamed), std::runtime_error);
    const capture::MenuClick missing[]{{99, "MOTION"}};
    CHECK_THROWS_AS(capture::apply_menu_clicks(controller, missing), std::runtime_error);
    CHECK(controller.simulation().motion_settings().profile == before);
}

TEST_CASE("busiest_window_start finds the window with the largest sum on the stride") {
    const std::vector<int> activity{0, 1, 5, 5, 1, 0, 4, 4, 4, 0};
    CHECK(capture::busiest_window_start(activity, 2, 1) == 2);
    CHECK(capture::busiest_window_start(activity, 3, 1) == 6);
    // Only every third index may start a window: 6 (4 + 4) beats 3 (5 + 1) and 0 (0 + 1).
    CHECK(capture::busiest_window_start(activity, 2, 3) == 6);
    // Equal sums: the earliest wins.
    CHECK(capture::busiest_window_start(std::vector<int>{2, 2, 2, 2}, 2, 1) == 0);
    CHECK(capture::busiest_window_start(activity, 10, 4) == 0);
    CHECK_THROWS_AS(capture::busiest_window_start(activity, 11, 1), std::invalid_argument);
    CHECK_THROWS_AS(capture::busiest_window_start(activity, 0, 1), std::invalid_argument);
    CHECK_THROWS_AS(capture::busiest_window_start(activity, 2, 0), std::invalid_argument);
}

TEST_CASE("driving_movers counts the captured floor's movers step by step, the same on every run") {
    constexpr int steps = 1200;
    const auto driving = capture::driving_movers(captured_floor(), steps);
    REQUIRE(driving.size() == steps);
    const auto movers = i4sim::load_blueprint(captured_floor()).movers.size();
    int busiest = 0;
    for (const int count : driving) {
        CHECK(count >= 0);
        CHECK(count <= static_cast<int>(movers));
        busiest = std::max(busiest, count);
    }
    // The workload sets movers off within the 20 s measured, but never all of them at once.
    CHECK(busiest > 0);
    CHECK(busiest < static_cast<int>(movers));
    CHECK(capture::driving_movers(captured_floor(), steps) == driving);
    CHECK(capture::driving_movers(captured_floor(), 0).empty());
    CHECK_THROWS_AS(capture::driving_movers(captured_floor(), -1), std::invalid_argument);
    CHECK_THROWS_AS(capture::driving_movers(captured_floor().parent_path() / "no-such-floor.json", 1),
                    std::runtime_error);
}

TEST_CASE("the Dotio debug clicks find their rows and turn on every layer but not the ghost camera") {
    dotio::Application application(capture::capture_seed);
    for (const auto &click : capture::dotio_debug_clicks) {
        // The rows are built anew for every click, as the session does: PATHS adds rows.
        const auto menu = application.debug_entries();
        REQUIRE_NOTHROW(capture::apply_menu_click(menu, click, "Dotio"));
    }
    CHECK(application.invulnerable);
    CHECK((application.vectors && application.probes && application.collisions && application.active_agents &&
           application.ai));
    for (std::size_t part = 0; part < application.paths.size(); ++part)
        CHECK_MESSAGE(application.paths[part], "PATHS part " << part << " is off");
    CHECK_FALSE(application.ghost);
    // Every check row of the LAYERS section the clicks leave open is on.
    bool in_layers = false;
    int layers = 0;
    for (const auto &row : application.debug_entries()) {
        if (row.depth == 0)
            in_layers = row.label == "LAYERS";
        else if (in_layers) {
            ++layers;
            CHECK_MESSAGE((row.kind == kinetra::DebugEntryKind::check && row.active()),
                          row.label << " is off");
        }
    }
    CHECK_MESSAGE(layers == 11, "the clicks cover every row of LAYERS");
}

TEST_CASE("the Demo debug click opens the motion section of a new session's menu") {
    demo::Application application(demo::default_agent_count, capture::capture_seed);
    demo::DebugPanel panel(application);
    const auto closed = panel.entries();
    REQUIRE_NOTHROW(capture::apply_menu_click(closed, capture::demo_motion_click, "Demo"));
    const auto open = panel.entries();
    REQUIRE(open.size() > 4);
    CHECK(open[2].label == "MOTION");
    CHECK(open[2].active());
    CHECK(open[3].label == "AVOIDANCE");
}

TEST_CASE("apply_menu_click rejects a row that is missing, renamed or without an action") {
    int clicks = 0;
    float speed = 1;
    const std::vector<kinetra::DebugEntry> menu{{.label = "RESET", .action = [&] { ++clicks; }},
                                                kinetra::debug_number("SPEED", speed, {0, 2, 1})};
    CHECK_NOTHROW(capture::apply_menu_click(menu, {0, "RESET"}, "test"));
    CHECK(clicks == 1);
    CHECK_THROWS_AS(capture::apply_menu_click(menu, {0, "RESTART"}, "test"), std::runtime_error);
    CHECK_THROWS_AS(capture::apply_menu_click(menu, {2, "RESET"}, "test"), std::runtime_error);
    CHECK_THROWS_AS(capture::apply_menu_click(menu, {1, "SPEED"}, "test"), std::runtime_error);
    CHECK(clicks == 1);
}

namespace {
/// A Dotio world without walls or enemies, and a camera on its player at clip size and the
/// game's own zoom: one legacy pixel is one screen pixel, and the view reaches 480 legacy
/// pixels to the sides and 270 up and down.
struct OpenDotioWorld {
    dotio::Application application{capture::capture_seed};
    kinetra::Camera2D camera;

    OpenDotioWorld() {
        world().grid().fill(0);
        for (auto &enemy : world().get_enemies())
            enemy.is_active = false;
        camera.viewport = {960, 540};
        camera.pixels_per_unit = 25;
        camera.position = dotio::to_world(player().position);
    }
    dotio::World &world() { return application.world(); }
    dotio::Player &player() { return world().get_player(); }
    /// Puts the enemy of pool slot `slot` at `offset` from the player, alive and simulated.
    dotio::Enemy &place(int slot, dotio::math::Vec2 offset, dotio::math::Vec2 velocity = {}) {
        auto &enemy = world().get_enemies()[slot];
        enemy.reset();
        enemy.frozen = false;
        enemy.position = player().position + offset;
        enemy.velocity = velocity;
        return enemy;
    }
    /// Blocks the grid cell that holds the point at `offset` from the player.
    void wall(dotio::math::Vec2 offset) {
        const auto point = player().position + offset;
        world().grid().set_cell(int(point.y / dotio::Grid::cell_size), int(point.x / dotio::Grid::cell_size),
                                1);
    }
};
} // namespace

TEST_CASE("lead_offset aims where a bolt meets a moving target, and at the target when it cannot") {
    constexpr float bolt = 6;
    SUBCASE("a resting target is aimed at directly") {
        const auto aim = capture::lead_offset({300, -40}, {}, bolt);
        CHECK(aim.x == doctest::Approx(300));
        CHECK(aim.y == doctest::Approx(-40));
    }
    SUBCASE("a crossing target is led to the point both reach at the same time") {
        const dotio::math::Vec2 offset{300, 0}, velocity{0, 2};
        const auto aim = capture::lead_offset(offset, velocity, bolt);
        CHECK(aim.x == doctest::Approx(300));
        CHECK(aim.y > 0);
        // The target needs as long to its part of the way as the bolt to the whole.
        const float target_ticks = (aim.y - offset.y) / velocity.y;
        CHECK(aim.get_length() / bolt == doctest::Approx(target_ticks));
    }
    SUBCASE("a target that outruns the bolt is aimed at directly") {
        const auto aim = capture::lead_offset({300, 0}, {7, 0}, bolt);
        CHECK(aim.x == doctest::Approx(300));
        CHECK(aim.y == doctest::Approx(0));
    }
    SUBCASE("a flight longer than the longest lead is not led") {
        // 5 px per tick closing speed against 600 px: 120 ticks.
        const auto aim = capture::lead_offset({600, 0}, {1, 0}, bolt);
        CHECK(600 / (bolt - 1) > capture::longest_lead_ticks);
        CHECK(aim.x == doctest::Approx(600));
    }
    SUBCASE("a target as fast as the bolt is caught only when it comes closer") {
        const auto approaching = capture::lead_offset({300, 0}, {-bolt, 0}, bolt);
        CHECK(approaching.x == doctest::Approx(150));
        const auto fleeing = capture::lead_offset({300, 0}, {bolt, 0}, bolt);
        CHECK(fleeing.x == doctest::Approx(300));
    }
}

TEST_CASE("the scripted Dotio player targets the nearest enemy it can see") {
    OpenDotioWorld world;
    CHECK(capture::dotio_target(world.application, world.camera) == nullptr);
    CHECK_FALSE(capture::dotio_aim(world.application, world.camera).has_value());

    auto &far = world.place(0, {300, 0});
    CHECK(capture::dotio_target(world.application, world.camera) == &far);
    // Nearer candidates that do not count: behind a wall, frozen, dead, and off screen.
    auto &hidden = world.place(1, {-200, 0});
    world.wall({-100, 0});
    world.place(2, {0, 120}).frozen = true;
    world.place(3, {0, -120}).is_active = false;
    world.place(4, {0, 280});
    CHECK(capture::dotio_target(world.application, world.camera) == &far);

    // Without the wall the hidden enemy is the nearest in sight.
    world.world().grid().fill(0);
    CHECK(capture::dotio_target(world.application, world.camera) == &hidden);
}

TEST_CASE("the scripted Dotio player does not shoot through a crate") {
    OpenDotioWorld world;
    // Twice as wide a view as the game's own, so the room's crate and the cell behind it fit.
    world.camera.pixels_per_unit = 12.5f;
    const auto player = world.player().position;
    const dotio::World &read_only = world.world();
    REQUIRE_FALSE(read_only.get_objects().empty());
    dotio::math::Vec2 crate{};
    float nearest = 400;
    for (const auto &object : read_only.get_objects())
        if (const auto offset = dotio::legacy_vector(object.body.position) - player;
            offset.get_length() < nearest) {
            crate = offset;
            nearest = offset.get_length();
        }
    REQUIRE_MESSAGE(nearest < 400, "the seeded world has no crate near the player's start");
    // Behind the crate on the same line, and as far away to the other side.
    world.place(0, crate * 1.4f);
    auto &open = world.place(1, crate * -1.5f);
    CHECK(capture::dotio_target(world.application, world.camera) == &open);
}

TEST_CASE("dotio_bolt_speed is the speed of the bolt the player fires") {
    OpenDotioWorld world;
    auto &player = world.player();
    for (const int kills : {1, 40}) {
        CAPTURE(kills);
        player.kills = kills;
        player.cooldown = 0;
        for (auto &bolt : world.world().get_bolts())
            bolt.is_active = false;
        player.shoot(player.position + dotio::math::Vec2{100, 0}, true);
        float fired = 0;
        for (auto &bolt : world.world().get_bolts())
            if (bolt.is_active)
                fired = bolt.velocity.get_length();
        CHECK(fired == doctest::Approx(capture::dotio_bolt_speed(player)));
    }
}

TEST_CASE("dotio_aim points at the target on screen, ahead of one that moves, inside the window") {
    OpenDotioWorld world;
    auto &enemy = world.place(0, {300, 0});
    auto aim = capture::dotio_aim(world.application, world.camera);
    REQUIRE(aim.has_value());
    CHECK(aim->x == doctest::Approx(480 + 300));
    CHECK(aim->y == doctest::Approx(270));

    // Legacy Y points down, as the screen's does: a target moving down is led downward.
    enemy.velocity = {0, 2};
    aim = capture::dotio_aim(world.application, world.camera);
    REQUIRE(aim.has_value());
    CHECK(aim->x == doctest::Approx(480 + 300));
    CHECK(aim->y > 270 + 50);

    // A lead that runs past the window's edge stops at the edge.
    enemy.position = world.player().position + dotio::math::Vec2{460, 0};
    enemy.velocity = {0.5f, 0};
    aim = capture::dotio_aim(world.application, world.camera);
    REQUIRE(aim.has_value());
    CHECK(aim->x == doctest::Approx(960));
}

TEST_CASE("dotio_recovery_in_view finds a recovering enemy on screen, inside the margin") {
    OpenDotioWorld world;
    auto &enemy = world.place(0, {300, 0});
    CHECK_FALSE(capture::dotio_recovery_in_view(world.application, world.camera, 0));
    enemy.set_state(enemy.get_recover_state());
    CHECK(capture::dotio_recovery_in_view(world.application, world.camera, 0));
    // 180 pixels from the right edge of the 960-pixel window.
    CHECK(capture::dotio_recovery_in_view(world.application, world.camera, 170));
    CHECK_FALSE(capture::dotio_recovery_in_view(world.application, world.camera, 190));
    enemy.position = world.player().position + dotio::math::Vec2{0, 400};
    CHECK_FALSE(capture::dotio_recovery_in_view(world.application, world.camera, 0));
}

namespace {
constexpr kinetra::Color white{1, 1, 1};

/// Whether `color` is the gray `level` in every channel, within one step of eight bits.
bool is_gray(kinetra::Color color, float level) {
    const auto near = [level](float channel) { return std::abs(channel - level) <= 1.5f / 255; };
    return near(color.r) && near(color.g) && near(color.b);
}
} // namespace

TEST_CASE("a canvas fills, blends and clips at its edges") {
    capture::Canvas canvas(8, 6);
    REQUIRE(canvas.rgb.size() == 8 * 6 * 3);
    capture::fill(canvas, {2, 1}, {3, 2}, {1, 0, 0});
    CHECK(canvas.pixel(2, 1).r == 1);
    CHECK(canvas.pixel(4, 2).r == 1);
    CHECK_MESSAGE(canvas.pixel(5, 2).r == 0, "the right edge is not part of the rectangle");
    CHECK_MESSAGE(canvas.pixel(2, 3).r == 0, "the bottom edge is not part of the rectangle");
    // Half-transparent blue over the red pixel mixes the two.
    capture::fill(canvas, {2, 1}, {1, 1}, {0, 0, 1, 0.5f});
    CHECK(canvas.pixel(2, 1).r == doctest::Approx(0.5).epsilon(0.02));
    CHECK(canvas.pixel(2, 1).b == doctest::Approx(0.5).epsilon(0.02));
    CHECK_NOTHROW(capture::fill(canvas, {-5, -5}, {100, 100}, {0, 1, 0}));
    CHECK(canvas.pixel(0, 0).g == 1);
    CHECK(canvas.pixel(7, 5).g == 1);
    CHECK_THROWS_AS(canvas.pixel(8, 0), std::out_of_range);
    CHECK_THROWS_AS(canvas.pixel(0, -1), std::out_of_range);
    CHECK_THROWS_AS(capture::Canvas(0, 4), std::invalid_argument);
}

TEST_CASE("canvas text draws the bitmap font in squares of its scale") {
    capture::Canvas canvas(40, 20);
    // I is 01110 on its top row and 00100 on the next one.
    capture::draw_text(canvas, "I", {2, 3}, 2, white);
    CHECK(canvas.pixel(2, 3).r == 0);
    CHECK(canvas.pixel(2 + 2, 3).r == 1);
    CHECK(canvas.pixel(2 + 3 * 2 + 1, 3 + 1).r == 1);
    CHECK(canvas.pixel(2 + 4 * 2, 3).r == 0);
    CHECK(canvas.pixel(2 + 2, 3 + 2).r == 0);
    CHECK(canvas.pixel(2 + 2 * 2, 3 + 2).r == 1);
    CHECK(capture::text_width("AB", 2) == (2 * 6 - 1) * 2);
    CHECK(capture::text_width("", 3) == 0);
    CHECK_NOTHROW(capture::draw_text(canvas, "CLIPPED AT THE EDGE", {30, 15}, 3, white));
}

TEST_CASE("the pointer hangs from its tip: a dark outline around a light body") {
    capture::Canvas canvas(60, 60);
    capture::fill(canvas, {0, 0}, {60, 60}, {0.5f, 0.5f, 0.5f});
    capture::draw_pointer(canvas, {10, 10});
    CHECK_MESSAGE(is_gray(canvas.pixel(10, 10), 0), "the tip is outline");
    // Fifth row of the shape: outline, three cells of body, outline; two pixels per cell.
    CHECK_MESSAGE(is_gray(canvas.pixel(10 + 2, 10 + 8), 1), "the body is light");
    CHECK_MESSAGE(is_gray(canvas.pixel(10 + 8, 10 + 8), 0), "the outline closes the body");
    CHECK_MESSAGE(is_gray(canvas.pixel(9, 10), 0.5f), "nothing is drawn left of the tip");
    CHECK_MESSAGE(is_gray(canvas.pixel(10 + capture::pointer_width, 10), 0.5f), "nor beyond its width");
    CHECK_NOTHROW(capture::draw_pointer(canvas, {55, 55}));
}

TEST_CASE("a ring covers its rim and leaves its middle and its surroundings alone") {
    capture::Canvas canvas(80, 80);
    capture::draw_ring(canvas, {40, 40}, 20, 4, white);
    CHECK(canvas.pixel(40 + 18, 40).r > 0.9f);
    CHECK(canvas.pixel(40, 40 - 18).r > 0.9f);
    CHECK(canvas.pixel(40, 40).r == 0);
    CHECK(canvas.pixel(40 + 10, 40).r == 0);
    CHECK(canvas.pixel(40 + 25, 40).r == 0);
}

TEST_CASE("a keycap is as wide as its label needs and fades with its strength") {
    CHECK(capture::keycap_width("W") == capture::text_width("W", 2) + 2 * capture::keycap_padding);
    CHECK(capture::keycap_width("SHIFT") > capture::keycap_width("W"));
    capture::Canvas full(120, 60), half(120, 60), none(120, 60);
    capture::draw_keycap(full, "W", {4, 4}, 1);
    capture::draw_keycap(half, "W", {4, 4}, 0.5f);
    capture::draw_keycap(none, "W", {4, 4}, 0);
    CHECK(full.pixel(4, 4).b > half.pixel(4, 4).b);
    CHECK(half.pixel(4, 4).b > 0);
    CHECK(none.pixel(4, 4).b == 0);
    CHECK_MESSAGE(full.pixel(4 + capture::keycap_width("W"), 4).b == 0, "nothing beyond the cap's width");
    CHECK_MESSAGE(full.pixel(4, 4 + capture::keycap_height).b == 0, "nothing below its height");
}

TEST_CASE("dimming scales the brightness of every pixel") {
    capture::Canvas canvas(4, 4);
    capture::fill(canvas, {0, 0}, {4, 4}, white);
    capture::dim(canvas, 0.5f);
    CHECK(is_gray(canvas.pixel(3, 3), 0.5f));
    capture::dim(canvas, 0);
    CHECK(is_gray(canvas.pixel(0, 0), 0));
}

TEST_CASE("the minimum jerk profile starts and ends at rest and never goes back") {
    CHECK(capture::minimum_jerk(0) == 0);
    CHECK(capture::minimum_jerk(1) == 1);
    CHECK(capture::minimum_jerk(0.5) == doctest::Approx(0.5));
    CHECK_MESSAGE(capture::minimum_jerk(0.1) < 0.02, "a move starts slowly");
    CHECK_MESSAGE(capture::minimum_jerk(0.9) > 0.98, "and ends slowly");
    CHECK(capture::minimum_jerk(-1) == 0);
    CHECK(capture::minimum_jerk(2) == 1);
    double previous = 0;
    for (int i = 1; i <= 100; ++i) {
        const double progress = capture::minimum_jerk(i / 100.0);
        CHECK(progress >= previous);
        previous = progress;
    }
}

TEST_CASE("a pointer path starts and ends on its points and stays near the straight line") {
    std::mt19937 random(7);
    const kinetra::Vec2 from{100, 100}, to{900, 500};
    const auto path = capture::plan_pointer_path(from, to, random);
    const auto same = [](kinetra::Vec2 a, kinetra::Vec2 b) { return a.x == b.x && a.y == b.y; };
    CHECK(same(path.at(0), from));
    CHECK(same(path.at(-1), from));
    CHECK(same(path.at(path.duration), to));
    CHECK(same(path.at(path.duration + 5), to));
    const float distance = kinetra::length(to - from);
    const kinetra::Vec2 along = (to - from) / distance;
    float covered = 0;
    for (int i = 1; i <= 60; ++i) {
        const kinetra::Vec2 offset = path.at(path.duration * i / 60) - from;
        const float progress = kinetra::dot(offset, along);
        CHECK_MESSAGE(progress >= covered - 0.001f, "the pointer went back at step " << i);
        covered = progress;
        // A quadratic curve leaves the straight line by at most half its control point's offset.
        const float sideways = std::abs(kinetra::dot(offset, kinetra::perpendicular(along)));
        CHECK(sideways <= distance * capture::largest_bend / 2 + 0.01f);
    }
}

TEST_CASE("a longer way takes longer, within limits, and no way takes no time") {
    std::mt19937 random(7);
    const auto near = capture::plan_pointer_path({0, 0}, {50, 0}, random);
    const auto middle = capture::plan_pointer_path({0, 0}, {400, 0}, random);
    const auto far = capture::plan_pointer_path({0, 0}, {1600, 0}, random);
    CHECK(near.duration < middle.duration);
    CHECK(middle.duration < far.duration);
    for (const auto &path : {near, middle, far}) {
        CHECK(path.duration >= capture::shortest_move);
        CHECK(path.duration <= capture::longest_move);
    }
    const auto none = capture::plan_pointer_path({30, 40}, {30, 40}, random);
    CHECK(none.duration == 0);
    CHECK(none.at(0).x == 30);
    CHECK(none.at(1).y == 40);
}

TEST_CASE("pointer paths repeat for a seed and bend otherwise for another") {
    std::mt19937 first(42), again(42), other(43);
    const auto a = capture::plan_pointer_path({0, 0}, {800, 300}, first);
    const auto b = capture::plan_pointer_path({0, 0}, {800, 300}, again);
    const auto c = capture::plan_pointer_path({0, 0}, {800, 300}, other);
    CHECK((a.control.x == b.control.x && a.control.y == b.control.y));
    CHECK((a.control.x != c.control.x || a.control.y != c.control.y));
    CHECK(a.duration == c.duration);
}

TEST_CASE("a row is found by its label and a missing label names the rows that are there") {
    const std::vector<kinetra_lab::DebugRow> rows{{"SIMULATION", 0}, {"RUNNING", 1}, {"ZOOM", 0}};
    CHECK(capture::row_with_label(rows, "SIMULATION") == 0);
    CHECK(capture::row_with_label(rows, "ZOOM") == 2);
    try {
        capture::row_with_label(rows, "MOTION");
        FAIL("a missing label was found");
    } catch (const std::runtime_error &error) {
        const std::string message = error.what();
        CHECK(message.find("'MOTION'") != std::string::npos);
        CHECK(message.find("SIMULATION, RUNNING, ZOOM") != std::string::npos);
    }
    try {
        capture::row_with_label({}, "MOTION");
        FAIL("a label was found in an empty menu");
    } catch (const std::runtime_error &error) {
        CHECK(std::string(error.what()).find("none") != std::string::npos);
    }
}
