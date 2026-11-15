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
void configure_debug(lab::Session &session, Runtime &runtime) {
    Input settings;
    settings.pressed[std::size_t(Key::control)] = true;
    session.advance(0, settings, runtime.camera);
    Input click;
    click.clicked = true;
    click.mouse = runtime.camera.viewport / 2 + (Vec2{50, 102} - lab::Settings{}.half_extent()) *
                                                    lab::Settings::scale(runtime.camera.viewport);
    session.advance(0, click, runtime.camera);
    session.advance(0, settings, runtime.camera);
}
void settings_regression(Runtime &runtime) {
    for (auto variant : {launcher::Variant::demo, launcher::Variant::dotio}) {
        auto session = launcher::start(variant, 0, 42, runtime.camera);
        const float initial_zoom = runtime.camera.pixels_per_unit;
        Input input;
        input.pressed[std::size_t(Key::space)] = true;
        input.wheel = 2;
        session->advance(0, input, runtime.camera);
        require(session->title().find("PAUSED") == std::string::npos &&
                    runtime.camera.pixels_per_unit == initial_zoom,
                "Space or normal wheel still active");
        input = {};
        input.pressed[std::size_t(Key::control)] = true;
        session->advance(0, input, runtime.camera);
        require(session->title().find("PAUSED") != std::string::npos, "Settings did not pause");
        session->render(runtime);
        lab::Settings layout{.sim_controls_height = variant == launcher::Variant::dotio ? 28.f : 42.f};
        auto click = [&](float x, float y) {
            Input pointer;
            pointer.clicked = pointer.mouse_down = true;
            pointer.mouse = runtime.camera.viewport / 2 + (Vec2{x, y} - layout.half_extent()) *
                                                              lab::Settings::scale(runtime.camera.viewport);
            session->advance(0, pointer, runtime.camera);
        };
        click(56, 46);
        require(runtime.camera.pixels_per_unit > initial_zoom, "Settings zoom failed");
        click(50, 74);
        click(50, 102);
        click(92, 134);
        layout.controls_visible = !layout.controls_visible;
        click(92, layout.debug_toggle_y());
        layout.debug_controls_visible = true;
        session->render(runtime);
        click(92, layout.debug_toggle_y());
        layout.debug_controls_visible = false;
        click(92, 134);
        layout.controls_visible = !layout.controls_visible;
        session->render(runtime);
        click(106, layout.actions_y());
        require(session->title().find("PAUSED") == std::string::npos, "Resume did not unpause");
        input = {};
        input.wheel = 1;
        const float zoom = runtime.camera.pixels_per_unit;
        session->advance(0, input, runtime.camera);
        require(runtime.camera.pixels_per_unit > zoom, "Debug wheel unavailable");
        input = {};
        input.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, input, runtime.camera);
        session->render(runtime);
        input = {};
        input.pressed[std::size_t(Key::m)] = true;
        session->advance(0, input, runtime.camera);
        require(!session->return_to_menu(), "M still returns to menu");
        input = {};
        input.pressed[std::size_t(Key::control)] = true;
        session->advance(0, input, runtime.camera);
        click(50, layout.actions_y());
        require(session->return_to_menu(), "Settings launcher action failed");
        click(146, layout.actions_y());
        require(session->exit_requested() && !session->close_on_escape(), "Settings exit policy failed");
        require(glGetError() == GL_NO_ERROR, "Settings rendering reported GL error");
    }
}
} // namespace
int main() {
    try {
        Runtime reopened("Launcher integration", 160, 120);
        SDL_Event escape{};
        escape.type = SDL_KEYDOWN;
        escape.key.keysym.sym = SDLK_ESCAPE;
        require(SDL_PushEvent(&escape) == 1, "Cannot inject Escape");
        require(reopened.poll(false) && reopened.input().hit(Key::escape),
                "Escape still shuts down the Lab runtime");
        escape.type = SDL_KEYUP;
        require(SDL_PushEvent(&escape) == 1 && reopened.poll(false), "Cannot release Escape");
        settings_regression(reopened);
        launcher::Menu menu;
        launcher::render_menu(reopened, menu);
        Input choose;
        choose.clicked = true;
        choose.mouse = launcher::MenuLayout(reopened.camera.viewport, 2).row_center(1);
        menu.update(choose, reopened.camera.viewport);
        launcher::render_menu(reopened, menu);
        auto session = launcher::start(launcher::Variant::demo, 0, 42, reopened.camera);
        Input pause;
        pause.pressed[std::size_t(Key::control)] = true;
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
        configure_debug(*session, reopened);
        Input debug_input;
        debug_input.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, debug_input, reopened.camera);
        debug_input = {};
        const float menu_scale = DebugMenu::scale(reopened.camera.viewport);
        debug_input.clicked = true;
        debug_input.mouse_down = true;
        debug_input.mouse.x = reopened.camera.viewport.x - 180 * menu_scale;
        // Exercise every debug layer and ensure the panel consumes held shooting.
        for (int row = 0; row < 11; ++row) {
            debug_input.mouse.y = (36 + row * 16) * menu_scale;
            session->advance(0, debug_input, reopened.camera);
        }
        session->render(reopened);
        session.reset();
        session = launcher::start(launcher::Variant::demo, 0, 42, reopened.camera);
        require(session->title().find("PAUSED") == std::string::npos && reopened.camera.pixels_per_unit == 17,
                "Returning to Demo did not create a clean session");
        session->advance(0, {}, reopened.camera);
        configure_debug(*session, reopened);
        Input demo_debug;
        demo_debug.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, demo_debug, reopened.camera);
        demo_debug = {};
        demo_debug.clicked = true;
        demo_debug.mouse = {reopened.camera.viewport.x - 180 * menu_scale, 36 * menu_scale};
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
