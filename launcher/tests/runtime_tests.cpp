#include "../include/session.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <kinetra/debug_view.h>
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
        Runtime reopened("Launcher integration", 160, 120);
        launcher::Menu menu;
        launcher::render_menu(reopened, menu);
        Input choose;
        choose.clicked = true;
        choose.mouse = launcher::MenuLayout(reopened.camera.viewport).row_center(1);
        menu.update(choose, reopened.camera.viewport);
        launcher::render_menu(reopened, menu);
        auto session = launcher::start(launcher::Variant::demo, 0, 42, reopened.camera);
        Input pause;
        pause.pressed[std::size_t(Key::space)] = true;
        session->advance(0, pause, reopened.camera);
        require(session->title().find("PAUSED") != std::string::npos,
                "Demo session did not load its bindings");
        session->render(reopened);
        session.reset();
        session = launcher::start(launcher::Variant::dotio, 0, 42, reopened.camera);
        session->advance(0, {}, reopened.camera);
        require(session->title().find("Dotio") != std::string::npos &&
                    session->title().find("PAUSED") == std::string::npos &&
                    reopened.camera.pixels_per_unit == 25,
                "Variant switch leaked pause/camera state");
        session->render(reopened);
        Input debug_input;
        debug_input.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, debug_input, reopened.camera);
        debug_input = {};
        const float menu_scale = DebugMenu::scale(reopened.camera.viewport);
        debug_input.clicked = true;
        debug_input.mouse_down = true;
        debug_input.mouse = {reopened.camera.viewport.x - 180 * menu_scale, 52 * menu_scale};
        session->advance(0, debug_input, reopened.camera);
        require(session->title().find("DEBUG") != std::string::npos,
                "Dotio debug menu did not enable debug mode");
        // Exercise every debug layer and ensure the panel consumes held shooting.
        for (int row = 2; row < 13; ++row) {
            debug_input.mouse.y = (36 + row * 16) * menu_scale;
            session->advance(0, debug_input, reopened.camera);
        }
        session->render(reopened);
        session.reset();
        session = launcher::start(launcher::Variant::demo, 0, 42, reopened.camera);
        require(session->title().find("PAUSED") == std::string::npos && reopened.camera.pixels_per_unit == 17,
                "Returning to Demo did not create a clean session");
        session->advance(0, {}, reopened.camera);
        Input demo_debug;
        demo_debug.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, demo_debug, reopened.camera);
        demo_debug = {};
        demo_debug.clicked = true;
        demo_debug.mouse = {reopened.camera.viewport.x - 180 * menu_scale, 52 * menu_scale};
        session->advance(0, demo_debug, reopened.camera);
        demo_debug.mouse.y = (36 + 2 * 16) * menu_scale;
        session->advance(0, demo_debug, reopened.camera);
        Input pan;
        pan.held[std::size_t(Key::right)] = true;
        session->advance(0.05, pan, reopened.camera);
        require(reopened.camera.position.x > 0 && session->title().find("camera FREE") != std::string::npos,
                "Demo debug Ghost did not route arrow input to camera");
        demo_debug = {};
        demo_debug.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, demo_debug, reopened.camera);
        Input teleport;
        teleport.clicked = true;
        teleport.mouse = reopened.camera.viewport / 2;
        session->advance(0, teleport, reopened.camera);
        require(session->title().find("teleported") != std::string::npos,
                "Demo Ghost click failed to teleport into free space");
        session->render(reopened);
        require(glGetError() == GL_NO_ERROR, "Menu or Dotio renderer reported an OpenGL error");
        for (const auto difficulty :
             {logic::Difficulty::easy, logic::Difficulty::medium, logic::Difficulty::hard}) {
            session = launcher::start(launcher::Variant::dotio, 0, 42, reopened.camera, difficulty);
            require(session->title().find(logic::settings_for(difficulty).name) != std::string::npos,
                    "Launcher lost difficulty when creating Dotio session");
        }
        session.reset();
        struct LoopSession final : Session {
            int &updates, &destroyed;
            LoopSession(int &u, int &d) : updates(u), destroyed(d) {}
            ~LoopSession() override { ++destroyed; }
            void advance(double elapsed, const Input &input, Camera2D &) override {
                require(elapsed == 0 && !input.hit(Key::enter), "Launcher selection leaked into session");
                ++updates;
            }
            void render(Runtime &runtime) override {
                runtime.begin();
                runtime.end();
                SDL_Event event{};
                event.type = SDL_KEYUP;
                event.key.keysym.sym = SDLK_RETURN;
                require(SDL_PushEvent(&event) == 1, "Cannot release selection key");
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_m;
                require(SDL_PushEvent(&event) == 1, "Cannot return to menu");
            }
            std::string title() override { return "LOOP TEST"; }
        };
        int updates = 0, destroyed = 0;
        SDL_Event enter{};
        enter.type = SDL_KEYDOWN;
        enter.key.keysym.sym = SDLK_RETURN;
        require(SDL_PushEvent(&enter) == 1, "Cannot select registered application");
        const int frames = run_launcher(
            reopened,
            {{{"REGISTERED", "TEST", {}, 0},
              [&](Camera2D &, std::size_t) { return std::make_unique<LoopSession>(updates, destroyed); }}},
            std::nullopt, 2);
        require(frames == 2 && updates == 1 && destroyed == 1,
                "Standard launcher did not create, isolate and destroy its session on M");
        std::cout << "PASS launcher menu, variants, debug menu and clean sessions\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
