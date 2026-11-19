// Window-free tests for kinetra_capture: the PNG writer (tools/capture/src/png.h), the
// scene list and the menu rows the scripts click (tools/capture/src/scenes.h), the showcase
// cycle measurement (tools/capture/src/showcase.h) and the scripted Dotio player
// (tools/capture/src/dotio_player.h). No Runtime or session is created.
#include "dotio_player.h"
#include "png.h"
#include "scenes.h"
#include "showcase.h"
#include "stage.h"

#include "../../../labs/i4sim/include/controller.h"
#include "../../../labs/i4sim/include/simulation.h"

#include <zlib.h>

#include <doctest/doctest.h>

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

TEST_CASE("every scene has its file in docs/media, the media guide and the gallery") {
    const auto readme = source_text("README.md"), guide = source_text("docs/media/README.md");
    for (const auto &scene : capture::scenes()) {
        const std::string file =
            std::string(scene.name) + (scene.kind == capture::SceneKind::clip ? ".gif" : ".png");
        CAPTURE(file);
        CHECK(std::filesystem::exists(std::filesystem::path(KINETRA_LAB_SOURCE_DIR) / "docs/media" / file));
        CHECK(guide.find("(" + file + ")") != std::string::npos);
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
                                                  "labs/i4sim/blueprints/MAS-demonstrator.json"),
                    std::invalid_argument);
    CHECK_THROWS_AS(capture::showcase_cycle_steps(showcase_file("no-such-scene")), std::runtime_error);
}

TEST_CASE("the i4sim menu clicks find their rows and choose the Reciprocal profile with plain A*") {
    i4sim::FactoryController controller(capture::capture_seed, false, capture::i4sim_updates_per_second);
    REQUIRE(controller.load(std::filesystem::path(KINETRA_LAB_SOURCE_DIR) / "labs/i4sim/blueprints" /
                            capture::i4sim_capture_blueprint));
    // The session shows the controller's entries as menu rows one to one, so a click on a
    // row runs the action of the entry with that index.
    for (const auto &click : capture::i4sim_motion_clicks) {
        auto menu = controller.menu();
        CAPTURE(click.label);
        REQUIRE(click.row < menu.size());
        REQUIRE(menu[click.row].label == click.label);
        menu[click.row].action();
    }
    const auto &motion = controller.simulation().motion_settings();
    CHECK(motion.profile == i4sim::SteeringProfile::reciprocal);
    CHECK_FALSE(motion.heatmap);
    const auto menu = controller.menu();
    REQUIRE(capture::i4sim_layers_click.row < menu.size());
    CHECK(menu[capture::i4sim_layers_click.row].label == capture::i4sim_layers_click.label);
}

TEST_CASE("the Dotio debug rows the scripts click turn on every row but GHOST CAMERA") {
    dotio::Application application(capture::capture_seed);
    for (std::size_t row = capture::dotio_first_debug_row; row <= capture::dotio_last_debug_row; ++row) {
        // The rows are built anew for every click, as the session does: PATHS adds rows.
        const auto options = application.debug_options();
        REQUIRE(row < options.size());
        REQUIRE(options[row].enabled != nullptr);
        *options[row].enabled = !*options[row].enabled;
    }
    const auto options = application.debug_options();
    REQUIRE(options.size() == capture::dotio_last_debug_row + 1);
    CHECK(options.front().label == "GHOST CAMERA");
    CHECK_FALSE(*options.front().enabled);
    CHECK(options.back().label == "INVULNERABLE");
    for (std::size_t row = capture::dotio_first_debug_row; row < options.size(); ++row)
        CHECK_MESSAGE(*options[row].enabled, options[row].label << " is off");
}

namespace {
/// A Dotio world without walls or enemies, and a camera on its player at clip size and the
/// game's own zoom: one legacy pixel is one screen pixel, and the view reaches 480 legacy
/// pixels to the sides and 270 up and down.
struct OpenDotioWorld {
    dotio::Application application{capture::capture_seed};
    kinetra::Camera2D camera;

    OpenDotioWorld() {
        for (auto &row : logic::Grid::Array)
            for (auto &cell : row)
                cell = 0;
        for (auto &enemy : world().GetEnemies())
            enemy.IsActive = false;
        camera.viewport = {960, 540};
        camera.pixels_per_unit = 25;
        camera.position = dotio::to_world(player().Position);
    }
    logic::World &world() { return application.world(); }
    logic::Player &player() { return world().GetPlayer(); }
    /// Puts the enemy of pool slot `slot` at `offset` from the player, alive and simulated.
    logic::Enemy &place(int slot, gmath::Vec2 offset, gmath::Vec2 velocity = {}) {
        auto &enemy = world().GetEnemies()[slot];
        enemy.Reset();
        enemy.Frozen = false;
        enemy.Position = player().Position + offset;
        enemy.Velocity = velocity;
        return enemy;
    }
    /// Blocks the grid cell that holds the point at `offset` from the player.
    void wall(gmath::Vec2 offset) {
        const auto point = player().Position + offset;
        logic::Grid::Array[int(point.Y / logic::Grid::CellSize)][int(point.X / logic::Grid::CellSize)] = 1;
    }
};
} // namespace

TEST_CASE("lead_offset aims where a bolt meets a moving target, and at the target when it cannot") {
    constexpr float bolt = 6;
    SUBCASE("a resting target is aimed at directly") {
        const auto aim = capture::lead_offset({300, -40}, {}, bolt);
        CHECK(aim.X == doctest::Approx(300));
        CHECK(aim.Y == doctest::Approx(-40));
    }
    SUBCASE("a crossing target is led to the point both reach at the same time") {
        const gmath::Vec2 offset{300, 0}, velocity{0, 2};
        const auto aim = capture::lead_offset(offset, velocity, bolt);
        CHECK(aim.X == doctest::Approx(300));
        CHECK(aim.Y > 0);
        // The target needs as long to its part of the way as the bolt to the whole.
        const float target_ticks = (aim.Y - offset.Y) / velocity.Y;
        CHECK(aim.GetLength() / bolt == doctest::Approx(target_ticks));
    }
    SUBCASE("a target that outruns the bolt is aimed at directly") {
        const auto aim = capture::lead_offset({300, 0}, {7, 0}, bolt);
        CHECK(aim.X == doctest::Approx(300));
        CHECK(aim.Y == doctest::Approx(0));
    }
    SUBCASE("a flight longer than the longest lead is not led") {
        // 5 px per tick closing speed against 600 px: 120 ticks.
        const auto aim = capture::lead_offset({600, 0}, {1, 0}, bolt);
        CHECK(600 / (bolt - 1) > capture::longest_lead_ticks);
        CHECK(aim.X == doctest::Approx(600));
    }
    SUBCASE("a target as fast as the bolt is caught only when it comes closer") {
        const auto approaching = capture::lead_offset({300, 0}, {-bolt, 0}, bolt);
        CHECK(approaching.X == doctest::Approx(150));
        const auto fleeing = capture::lead_offset({300, 0}, {bolt, 0}, bolt);
        CHECK(fleeing.X == doctest::Approx(300));
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
    world.place(2, {0, 120}).Frozen = true;
    world.place(3, {0, -120}).IsActive = false;
    world.place(4, {0, 280});
    CHECK(capture::dotio_target(world.application, world.camera) == &far);

    // Without the wall the hidden enemy is the nearest in sight.
    for (auto &row : logic::Grid::Array)
        for (auto &cell : row)
            cell = 0;
    CHECK(capture::dotio_target(world.application, world.camera) == &hidden);
}

TEST_CASE("dotio_aim points at the target on screen, ahead of one that moves, inside the window") {
    OpenDotioWorld world;
    auto &enemy = world.place(0, {300, 0});
    auto aim = capture::dotio_aim(world.application, world.camera);
    REQUIRE(aim.has_value());
    CHECK(aim->x == doctest::Approx(480 + 300));
    CHECK(aim->y == doctest::Approx(270));

    // Legacy Y points down, as the screen's does: a target moving down is led downward.
    enemy.Velocity = {0, 2};
    aim = capture::dotio_aim(world.application, world.camera);
    REQUIRE(aim.has_value());
    CHECK(aim->x == doctest::Approx(480 + 300));
    CHECK(aim->y > 270 + 50);

    // A lead that runs past the window's edge stops at the edge.
    enemy.Position = world.player().Position + gmath::Vec2{460, 0};
    enemy.Velocity = {0.5f, 0};
    aim = capture::dotio_aim(world.application, world.camera);
    REQUIRE(aim.has_value());
    CHECK(aim->x == doctest::Approx(960));
}

TEST_CASE("dotio_recovery_in_view finds a recovering enemy on screen, inside the margin") {
    OpenDotioWorld world;
    auto &enemy = world.place(0, {300, 0});
    CHECK_FALSE(capture::dotio_recovery_in_view(world.application, world.camera, 0));
    enemy.SetState(enemy.GetRecoverState());
    CHECK(capture::dotio_recovery_in_view(world.application, world.camera, 0));
    // 180 pixels from the right edge of the 960-pixel window.
    CHECK(capture::dotio_recovery_in_view(world.application, world.camera, 170));
    CHECK_FALSE(capture::dotio_recovery_in_view(world.application, world.camera, 190));
    enemy.Position = world.player().Position + gmath::Vec2{0, 400};
    CHECK_FALSE(capture::dotio_recovery_in_view(world.application, world.camera, 0));
}
