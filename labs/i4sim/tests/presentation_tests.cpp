#include "session.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <glad/glad.h>
#include <iostream>
#include <kinetra/launcher.h>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        using namespace kinetra;
        Runtime runtime("i4sim presentation", 640, 480);
        auto session = i4sim::create_session();
        session->advance(0, {}, runtime.camera);
        session->render(runtime);
        require(session->title().find("Loadscreen") != std::string::npos, "Missing loadscreen");
        Input choose;
        choose.pressed[std::size_t(Key::enter)] = true;
        session->advance(0, choose, runtime.camera);
        session->render(runtime);
        require(session->title().find("PAUSED") != std::string::npos,
                "Loadscreen did not load paused blueprint");
        Input run;
        run.pressed[std::size_t(Key::space)] = true;
        session->advance(0, run, runtime.camera);
        for (Key key : {Key::one, Key::two, Key::three, Key::four, Key::five, Key::six}) {
            Input toggle;
            toggle.pressed[std::size_t(key)] = true;
            session->advance(1.0 / 60, toggle, runtime.camera);
        }
        session->render(runtime);
        require(session->title().find("RUNNING") != std::string::npos, "Pause control failed");
        SDL_Event event{};
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_RIGHT;
        require(SDL_PushEvent(&event) == 1 && runtime.poll(false) && runtime.input().right_clicked &&
                    runtime.input().right_mouse_down,
                "Right button was not exposed");
        require(runtime.poll(false) && !runtime.input().right_clicked && runtime.input().right_mouse_down,
                "Right click was not frame-scoped");
        event.type = SDL_MOUSEBUTTONUP;
        require(SDL_PushEvent(&event) == 1 && runtime.poll(false) && !runtime.input().right_mouse_down,
                "Right button did not release");
        if (const char *capture = std::getenv("I4SIM_CAPTURE")) {
            int width, height;
            SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &width, &height);
            std::vector<unsigned char> pixels(width * height * 3);
            glReadBuffer(GL_FRONT);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            std::ofstream file(capture, std::ios::binary);
            file << "P6\n" << width << " " << height << "\n255\n";
            for (int y = height - 1; y >= 0; --y)
                file.write(reinterpret_cast<const char *>(pixels.data() + y * width * 3), width * 3);
        }
        require(glGetError() == GL_NO_ERROR, "i4sim rendering produced OpenGL error");
        Input panel;
        panel.pressed[std::size_t(Key::control)] = true;
        session->advance(0, panel, runtime.camera);
        require(session->title().find("PAUSED") != std::string::npos, "Settings did not suspend simulation");
        Input suppressed;
        suppressed.pressed[std::size_t(Key::space)] = true;
        suppressed.clicked = true;
        suppressed.mouse = {1, 479};
        session->advance(1, suppressed, runtime.camera);
        require(session->title().find("PAUSED") != std::string::npos, "Modal settings leaked simulation input");
        session->advance(0, panel, runtime.camera);
        require(session->title().find("RUNNING") != std::string::npos, "Settings did not restore running state");
        Input back;
        back.pressed[std::size_t(Key::control)] = true;
        session->advance(0, back, runtime.camera);
        session->render(runtime);
        back = {};
        // Centered modal settings, default collapsed help: launcher action.
        const float scale = std::min({2.f, runtime.camera.viewport.x / 204.f,
                                      runtime.camera.viewport.y / 418.f});
        back.mouse = runtime.camera.viewport / 2 + Vec2{-41, 67} * scale;
        back.clicked = true;
        session->advance(0, back, runtime.camera);
        require(session->return_to_menu(), "Cannot return to launcher");
        session = i4sim::create_session(42, i4sim::default_blueprint_directory(),
                                        i4sim::default_blueprint_directory() / "missing.json");
        session->render(runtime);
        require(session->title().find("Loadscreen") != std::string::npos,
                "Failed load left a partial simulation");
        const auto repaired_catalog =
            std::filesystem::temp_directory_path() /
            ("i4sim-catalog-recovery-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        session = i4sim::create_session(42, repaired_catalog);
        require(session->title().find(repaired_catalog.string()) != std::string::npos,
                "Missing catalog error does not identify its directory");
        session->render(runtime);
        std::filesystem::create_directories(repaired_catalog);
        std::filesystem::copy_file(i4sim::default_blueprint_directory() / "MAS-demonstrator.json",
                                   repaired_catalog / "factory.json");
        session->advance(0, choose, runtime.camera);
        require(session->title() == "i4sim | Loadscreen", "Enter did not rediscover a repaired catalog");
        session->advance(0, choose, runtime.camera);
        require(session->title().find("PAUSED") != std::string::npos,
                "Repaired catalog did not load its blueprint");
        session->render(runtime);
        require(glGetError() == GL_NO_ERROR, "Catalog recovery rendering produced an OpenGL error");
        std::cout << "PASS loadscreen, simulation overlays, input and session lifecycle\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
