// Window-free tests for kinetra_capture: the PNG writer (tools/capture/src/png.h) and the
// scene list (tools/capture/src/scenes.h). No Runtime or session is created.
#include "png.h"
#include "scenes.h"

#include <zlib.h>

#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
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

TEST_CASE("the scenes cover every file in docs/media once") {
    std::set<std::string> names;
    for (const auto &scene : capture::scenes()) {
        CHECK_MESSAGE(names.insert(std::string(scene.name)).second, scene.name << " appears twice");
        CHECK(scene.script != nullptr);
        CHECK((scene.width > 0 && scene.height > 0));
        CHECK((scene.kind == capture::SceneKind::clip) == scene.name.ends_with("-clip"));
    }
    CHECK(names == std::set<std::string>{"launcher", "launcher-difficulty", "launcher-loadscreen", "demo",
                                         "demo-debug", "demo-clip", "demo-debug-clip", "dotio", "dotio-debug",
                                         "dotio-settings", "dotio-clip", "dotio-debug-clip", "i4sim",
                                         "i4sim-debug", "i4sim-clip", "i4sim-debug-clip"});
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
