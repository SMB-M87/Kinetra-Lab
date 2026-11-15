#include "../include/physics_adapter.h"
#include "../include/presentation.h"
#include <glad/glad.h>
#include <iostream>
#include <kinetra/engine.h>
#include <stdexcept>
#include <vector>
using namespace kinetra;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
// Inspect the statistics area in both swap buffers, including offscreen SDL backends.
int statistics_pixels(std::vector<bool> *rows = nullptr) {
    glFinish();
    int viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int width = viewport[2] / 4, height = viewport[3];
    if (rows)
        rows->assign(height, false);
    std::vector<unsigned char> pixels(width * height * 4);
    int count = 0;
    for (const auto buffer : {GL_FRONT, GL_BACK}) {
        glReadBuffer(buffer);
        glReadPixels(viewport[2] - width, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        for (std::size_t i = 0; i < pixels.size(); i += 4)
            if (pixels[i] > 200 && pixels[i + 1] > 200 && pixels[i + 2] < 80) {
                ++count;
                if (rows)
                    (*rows)[i / (width * 4)] = true;
            }
    }
    return count;
}
// Count player-colored pixels at the camera center in both swap buffers.
int ghost_marker_pixels() {
    glFinish();
    int viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    int count = 0;
    std::vector<unsigned char> pixels(32 * 32 * 4);
    for (const auto buffer : {GL_FRONT, GL_BACK}) {
        glReadBuffer(buffer);
        glReadPixels(viewport[2] / 2 - 16, viewport[3] / 2 - 16, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
        for (std::size_t i = 0; i < pixels.size(); i += 4)
            count += pixels[i] > 80 && pixels[i] < 125 && pixels[i + 1] < 20 && pixels[i + 2] > 240;
    }
    return count;
}
int statistics_bands() {
    std::vector<bool> rows;
    statistics_pixels(&rows);
    int bands = 0;
    bool previous = false;
    for (const bool row : rows) {
        bands += row && !previous;
        previous = row;
    }
    return bands;
}
} // namespace
int main() {
    try {
        const auto first_color = dotio::wall_color(-120, 30);
        const auto distant_color = dotio::wall_color(120, -30);
        require(std::abs(first_color.r - distant_color.r) + std::abs(first_color.g - distant_color.g) > 0.05f,
                "Walls lost their spatial color variation");
        for (int row = -200; row <= 200; ++row) {
            const auto color = dotio::wall_color(row, row * 2);
            const auto next = dotio::wall_color(row + 1, row * 2 + 1);
            require(color.r >= 0.19f && color.g >= 0.21f && color.b >= 0.26f && color.r <= 0.45f &&
                        color.g <= 0.51f && color.b <= 0.58f && color.a == 1,
                    "Wall palette lost contrast against the black background");
            require(std::abs(color.r - next.r) < 0.02f && std::abs(color.g - next.g) < 0.02f &&
                        std::abs(color.b - next.b) < 0.02f,
                    "Wall gradient has a color seam");
        }
        Runtime reopened("Dotio presentation", 320, 240);
        Input controls;
        controls.pressed[std::size_t(Key::space)] = true;
        controls.held[std::size_t(Key::shift)] = true;
        controls.clicked = controls.mouse_down = true;
        controls.mouse = {150, 200};
        controls.wheel = 2;
        controls.held[std::size_t(Key::right)] = true;
        controls.held[std::size_t(Key::w)] = true;
        const auto mapped = dotio::read_commands(controls, reopened.camera);
        require(mapped.pause && mapped.teleport && mapped.shoot && mapped.zoom_steps == 2,
                "Dotio pause/teleport/shoot/wheel mapping failed");
        require(mapped.direction.x == 1 && mapped.direction.y == 1 && mapped.camera_pan.x == 1,
                "Dotio WASD and arrows must share movement intent");
        dotio::Application application(42);
        application.advance(0, {}, reopened.camera);
        dotio::render(reopened, application);
        require(statistics_pixels() == 0, "Disabled statistics rendered outside debug mode");
        // Submitted geometry must stay independent of the offscreen resident population.
        const auto geometry_bytes = [&]() {
            dotio::render(reopened, application);
            GLint bytes = 0;
            glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bytes);
            return bytes;
        };
        const auto baseline_bytes = geometry_bytes();
        auto &enemy = application.world().GetEnemies()[0];
        enemy.IsActive = true;
        enemy.Position = dotio::from_world(reopened.camera.position + Vec2{1000, 1000});
        require(geometry_bytes() == baseline_bytes, "Offscreen enemy still submits circle geometry");
        const float half_width = reopened.camera.viewport.x / (2 * reopened.camera.pixels_per_unit);
        enemy.Position = dotio::from_world(reopened.camera.position + Vec2{half_width + 0.2f, 0});
        require(geometry_bytes() > baseline_bytes, "Culling removed an edge-crossing enemy circle");
        enemy.IsActive = false;
        auto &arrow = application.world().GetArrows()[0];
        arrow.IsActive = true;
        arrow.Velocity = {1, 0};
        arrow.Position = dotio::from_world(reopened.camera.position + Vec2{1000, 1000});
        require(geometry_bytes() == baseline_bytes, "Offscreen arrow still submits segment geometry");
        arrow.Position = dotio::from_world(reopened.camera.position + Vec2{half_width + 0.2f, 0});
        require(geometry_bytes() > baseline_bytes, "Culling removed an edge-crossing arrow segment");
        arrow.IsActive = false;

        application.statistics = true;
        dotio::render(reopened, application);
        require(!application.debug && statistics_pixels() > 0,
                "Enabled statistics are hidden outside debug mode");
        require(statistics_bands() == 1, "Normal statistics is not a single row");
        application.debug = true;
        dotio::render(reopened, application);
        dotio::render(reopened, application);
        require(statistics_bands() == 2, "Agent counts are not on a separate row above timing statistics");
        application.debug = false;
        application.statistics = false;
        dotio::render(reopened, application);
        dotio::render(reopened, application);
        require(statistics_pixels() == 0, "Statistics toggle did not hide the overlay");
        application.debug = application.vectors = application.collisions = application.ai = true;
        application.probes = application.statistics = true;
        application.paths.fill(true);
        dotio::render(reopened, application);
        require(glGetError() == GL_NO_ERROR, "Dotio debug rendering failed");
        application.debug = application.ghost = true;
        application.world().GetGhost().Position = application.world().GetPlayer().Position;
        application.world().GetGhost().Position.X += 1000;
        reopened.camera.position = dotio::to_world(application.world().GetGhost().Position);
        dotio::render(reopened, application);
        dotio::render(reopened, application);
        require(ghost_marker_pixels() == 0, "Ghost camera still draws a purple player marker");
        std::cout << "PASS Dotio input and presentation\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
