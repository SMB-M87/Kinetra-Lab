// Graphics tests for the Demo presentation: help overlay placement, routes and the
// renderer, read back from a real OpenGL context. Every check shares one Runtime,
// because only one may exist at a time, so they form one case. CTest labels it `graphics`.
#include "demo/presentation/help_overlay.h"
#include "demo/presentation/sandbox_renderer.h"

#include <kinetra/runtime/engine.h>

#include <SDL2/SDL.h>
#include <doctest/doctest.h>
#include <glad/glad.h>

#include <string>

using namespace kinetra;

TEST_CASE("help overlay routes and steering probes render at every camera zoom") {
    Runtime reopened("Demo graphics", 160, 120);
    reopened.camera.position = {321, -123};
    reopened.camera.pixels_per_unit = 73;
    for (bool expanded : {true, false}) {
        CAPTURE(expanded);
        reopened.begin({1, 0, 0, 1});
        sandbox::draw_help(reopened, expanded);
        reopened.end();
        glFinish();
        unsigned char pixel[4]{};
        // The panel at logical (4,4) must cover the red clear color even
        // with a translated, zoomed camera and a very small window.
        auto *window = SDL_GL_GetCurrentWindow();
        int width, height, logical_width, logical_height;
        SDL_GL_GetDrawableSize(window, &width, &height);
        SDL_GetWindowSize(window, &logical_width, &logical_height);
        const auto read_panel = [&] {
            glReadPixels(4 * width / logical_width, height - 1 - 4 * height / logical_height, 1, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel);
        };
        glReadBuffer(GL_FRONT);
        read_panel();
        if (pixel[0] > 20 || pixel[2] < 10) {
            glReadBuffer(GL_BACK);
            read_panel();
        }
        CHECK_MESSAGE((pixel[0] < 20 && pixel[2] >= 10), "Help panel missing at screen anchor");
        CHECK_MESSAGE((reopened.camera.position.x == 321 && reopened.camera.position.y == -123 &&
                       reopened.camera.pixels_per_unit == 73),
                      "Help overlay changed camera");
    }
    CHECK_MESSAGE(glGetError() == GL_NO_ERROR, "Help overlay rendering failed");
    sandbox::SandboxApplication application(0, 42);
    reopened.camera.position = {};
    application.advance(0, {}, reopened.camera);
    sandbox::render(reopened, application);
    sandbox::FrameCommands debug;
    debug.toggle_paths = debug.toggle_vectors = debug.toggle_help = true;
    debug.pointer =
        sandbox::PointerCommand{sandbox::PointerAction::walk, reopened.camera.world_to_screen({0, -17})};
    application.advance(0, debug, reopened.camera);
    REQUIRE_MESSAGE(application.last_action() == sandbox::ActionResult::walking,
                    "Missing route for renderer check");
    sandbox::render(reopened, application);
    CHECK_MESSAGE(glGetError() == GL_NO_ERROR, "Sandbox renderer reported an OpenGL error");
    sandbox::FrameCommands probes;
    probes.probes = true;
    probes.toggle_paths = probes.toggle_vectors = true;
    application.advance(0, probes, reopened.camera);
    const auto &player = application.scene().world.body(application.scene().player);
    for (float zoom : {5.f, 73.f}) {
        CAPTURE(zoom);
        reopened.camera.pixels_per_unit = zoom;
        reopened.camera.position = player.position + rotated(Vec2{3, 0}, player.angle);
        sandbox::render(reopened, application);
        glFinish();
        unsigned char pixel[4]{};
        int width, height;
        SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &width, &height);
        auto read_probe = [&] {
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        };
        glReadBuffer(GL_FRONT);
        read_probe();
        if (pixel[0] < 240 || pixel[1] < 150 || pixel[1] > 180) {
            glReadBuffer(GL_BACK);
            read_probe();
        }
        CHECK_MESSAGE((pixel[0] > 240 && pixel[1] >= 150 && pixel[1] <= 180),
                      "Steering probe disappeared at camera zoom");
    }
}
