#include "../include/session.h"
#include "../../labs/i4sim/include/factory_settings.h"
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
template <typename Settings> void full_debug_label(Runtime &runtime) {
    Settings settings;
    runtime.begin({0, 0, 0});
    settings.draw(runtime, false, true, false);
    runtime.end();
    glFinish();
    int width, height;
    SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &width, &height);
    unsigned char pixel[4]{};
    auto read = [&] {
        glReadPixels(int(98.5f * width / runtime.camera.viewport.x),
                     height - 1 - int(8.5f * height / runtime.camera.viewport.y),
                     1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    };
    glReadBuffer(GL_FRONT);
    read();
    if (pixel[0] < 200) {
        glReadBuffer(GL_BACK);
        read();
    }
    require(pixel[0] > 200 && pixel[1] > 200, "DEBUG button is missing its final G");
}
void default_debug_views() {
    Runtime runtime("Default debug views", 640, 480);
    auto *window = SDL_GL_GetCurrentWindow();
    runtime.poll(false);
    auto pixel = [&](Vec2 point) {
        int width, height;
        SDL_GL_GetDrawableSize(window, &width, &height);
        std::array<unsigned char, 4> color{};
        glReadPixels(int(point.x * width / runtime.camera.viewport.x),
                     height - 1 - int(point.y * height / runtime.camera.viewport.y),
                     1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
        return color;
    };
    for (auto variant : {launcher::Variant::demo, launcher::Variant::i4sim}) {
        auto session = launcher::start(variant, 0, 42, runtime.camera);
        if (variant == launcher::Variant::i4sim) {
            Input choose;
            choose.pressed[std::size_t(Key::enter)] = true;
            session->advance(0, choose, runtime.camera);
        }
        Input menu;
        menu.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, menu, runtime.camera);
        session->render(runtime);
        glFinish();
        const float scale = DebugMenu::scale(runtime.camera.viewport);
        const auto checkbox = [&](int row) {
            return Vec2{8, 32} + Vec2{12.5f, 29.5f + row * 16} * scale;
        };
        glReadBuffer(GL_FRONT);
        if (pixel(checkbox(1))[0] < 150)
            glReadBuffer(GL_BACK);
        for (int row = 1; row <= (variant == launcher::Variant::demo ? 5 : 7); ++row) {
            const auto color = pixel(checkbox(row));
            require(color[0] > 150, ("Visual debug layer did not start enabled: variant " +
                    std::to_string(int(variant)) + " row " + std::to_string(row) + " red " +
                    std::to_string(color[0]) + " viewport " + std::to_string(runtime.camera.viewport.x)).c_str());
        }
        if (variant == launcher::Variant::demo) {
            require(pixel(checkbox(8))[0] > 150, "Demo reciprocal avoidance did not start enabled");
            for (int row : {0, 6, 7})
                require(pixel(checkbox(row))[0] < 100, "Demo debug default changed camera, goals or gravity");
            const auto statistics = pixel({10.5f, runtime.camera.viewport.y - 19.5f});
            require(statistics[0] > 200 && statistics[1] > 200 && statistics[2] < 100,
                    "Demo statistics did not start enabled");
        }
    }
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
        session->advance(0, input, runtime.camera);
        require(session->title().find("PAUSED") == std::string::npos, "Space still pauses gameplay");
        input = {};
        input.wheel = 2;
        session->advance(0, input, runtime.camera);
        require(variant == launcher::Variant::demo ? runtime.camera.pixels_per_unit > initial_zoom
                                                  : runtime.camera.pixels_per_unit == initial_zoom,
                "Demo debug must start enabled; Dotio debug must remain opt-in");
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
        if (variant == launcher::Variant::dotio)
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
        input.clicked = input.mouse_down = true;
        input.mouse = {80, 12};
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
        default_debug_views();
        Runtime reopened("Launcher integration", 160, 120);
        SDL_Event escape{};
        escape.type = SDL_KEYDOWN;
        escape.key.keysym.sym = SDLK_ESCAPE;
        require(SDL_PushEvent(&escape) == 1, "Cannot inject Escape");
        require(reopened.poll(false) && reopened.input().hit(Key::escape),
                "Escape still shuts down the Lab runtime");
        escape.type = SDL_KEYUP;
        require(SDL_PushEvent(&escape) == 1 && reopened.poll(false), "Cannot release Escape");
        full_debug_label<lab::Settings>(reopened);
        full_debug_label<i4sim::FactorySettings>(reopened);
        settings_regression(reopened);
        launcher::Menu menu;
        launcher::render_menu(reopened, menu);
        Input choose;
        choose.clicked = true;
        choose.mouse = launcher::MenuLayout(reopened.camera.viewport, 3).row_center(1);
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
        debug_input.mouse.x = 8 + 10 * menu_scale;
        // Exercise every debug layer and ensure the panel consumes held shooting.
        for (int row = 0; row < 11; ++row) {
            debug_input.mouse.y = 32 + (28 + row * 16) * menu_scale;
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
        demo_debug.mouse = {8 + 10 * menu_scale, 32 + 28 * menu_scale};
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
        session = launcher::start(launcher::Variant::i4sim, 0, 42, reopened.camera);
        session->advance(0, {}, reopened.camera);
        session->render(reopened);
        require(session->title().find("Loadscreen") != std::string::npos,
                "Factory variant skipped loadscreen");
        Input blueprint_choice;
        blueprint_choice.pressed[std::size_t(Key::enter)] = true;
        session->advance(0, blueprint_choice, reopened.camera);
        session->render(reopened);
        require(session->title().find("PAUSED") != std::string::npos,
                "Factory blueprint did not start paused");
        session = launcher::start(launcher::Variant::demo, 0, 42, reopened.camera);
        require(reopened.camera.pixels_per_unit == 17 && session->title().find("PAUSED") == std::string::npos,
                "Factory state leaked into Demo");
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
