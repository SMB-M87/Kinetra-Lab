#include "../include/help_overlay.h"
#include "../include/sandbox_renderer.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <kinetra/engine.h>
#include <stdexcept>
using namespace kinetra;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        Runtime reopened("Demo graphics", 160, 120);
        reopened.camera.position = {321, -123};
        reopened.camera.pixels_per_unit = 73;
        for (bool expanded : {true, false}) {
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
                glReadPixels(4 * width / logical_width, height - 1 - 4 * height / logical_height, 1, 1,
                             GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            };
            glReadBuffer(GL_FRONT);
            read_panel();
            if (pixel[0] > 20 || pixel[2] < 10) {
                glReadBuffer(GL_BACK);
                read_panel();
            }
            require(pixel[0] < 20 && pixel[2] >= 10, "Help panel missing at screen anchor");
            require(reopened.camera.position.x == 321 && reopened.camera.position.y == -123 &&
                        reopened.camera.pixels_per_unit == 73,
                    "Help overlay changed camera");
        }
        require(glGetError() == GL_NO_ERROR, "Help overlay rendering failed");
        sandbox::SandboxApplication application(0, 42);
        reopened.camera.position = {};
        application.advance(0, {}, reopened.camera);
        sandbox::render(reopened, application);
        sandbox::FrameCommands debug;
        debug.toggle_paths = debug.toggle_vectors = debug.toggle_help = true;
        debug.pointer =
            sandbox::PointerCommand{sandbox::PointerAction::walk, reopened.camera.world_to_screen({0, -17})};
        application.advance(0, debug, reopened.camera);
        require(application.last_action() == sandbox::ActionResult::walking,
                "Missing route for renderer check");
        sandbox::render(reopened, application);
        require(glGetError() == GL_NO_ERROR, "Sandbox renderer reported an OpenGL error");
        require(sandbox::window_title(application).find("H help") != std::string::npos,
                "Sandbox title lost help controls");
        std::cout << "PASS Demo help, routes and renderer\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
