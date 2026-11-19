// Graphics integration tests for the bundled launcher: the real Lab sessions, created
// through launcher::applications(), with an OpenGL context. They need
// KINETRA_TEST_GRAPHICS; only one Runtime exists at a time, so every case owns its own.
#include "../../labs/dotio/include/session.h"
#include "../include/applications.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <kinetra/debug_view.h>
#include <kinetra/engine.h>
#include <kinetra/settings_view.h>
#include <lab/session.h>
#include <lab/settings.h>

#include <doctest/doctest.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

using namespace kinetra;
namespace {
using launcher::Variant;

/// Creates the session of `variant` exactly as the launcher does, including the camera reset.
std::unique_ptr<Session> start(Variant variant, Camera2D &camera,
                               std::size_t difficulty = dotio::default_difficulty) {
    const launcher::Options options{0, 42, difficulty};
    auto registrations = launcher::applications(options);
    const auto selection = launcher::selection(variant, options);
    return registrations.at(selection.application).create(camera, selection.configuration);
}

/// The state summary of a Lab session; every launcher registration creates one.
std::string status(Session &session) {
    return dynamic_cast<lab::Session &>(session).status();
}

bool paused(Session &session) {
    return status(session).find("PAUSED") != std::string::npos;
}

/// Reads one pixel at a logical window position from whichever buffer holds the frame.
std::array<unsigned char, 4> pixel(Runtime &runtime, Vec2 point) {
    int width, height;
    SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &width, &height);
    std::array<unsigned char, 4> color{};
    auto read = [&] {
        glReadPixels(int(point.x * float(width) / runtime.camera.viewport.x),
                     height - 1 - int(point.y * float(height) / runtime.camera.viewport.y), 1, 1, GL_RGBA,
                     GL_UNSIGNED_BYTE, color.data());
    };
    glReadBuffer(GL_FRONT);
    read();
    if (color[0] < 150 && color[1] < 150) {
        glReadBuffer(GL_BACK);
        read();
    }
    return color;
}

/// A panel with the same rows as a Lab session's, used to address its buttons and to
/// mirror which help sections are open.
struct PanelLayout {
    bool statistics{}, debug{}, frame_times{};
    SettingsOverlay overlay;
    Camera2D camera;
    explicit PanelLayout(std::size_t sim_lines, Vec2 viewport)
        : overlay(lab::settings_rows([] { return ZoomLimits{1, 2}; }, statistics, debug, frame_times,
                                     std::vector<std::string>(sim_lines, "LINE"), {"LINE"})) {
        camera.viewport = viewport;
        Input open;
        open.pressed[std::size_t(Key::control)] = true;
        overlay.update(open, camera);
    }
    PanelLayout(const PanelLayout &) = delete;
    PanelLayout &operator=(const PanelLayout &) = delete;
    /// The click as the session's panel receives it; the mirror applies it too.
    Input click(float x, float y) {
        Input pointer;
        pointer.clicked = pointer.mouse_down = true;
        pointer.mouse = camera.viewport / 2 +
                        (Vec2{x, y} - overlay.half_extent()) * SettingsOverlay::scale(camera.viewport);
        overlay.update(pointer, camera);
        return pointer;
    }
};
constexpr std::size_t sim_row = 4, debug_controls_row = 5;
} // namespace

TEST_CASE("the debug toolbar button shows its full label") {
    Runtime runtime("Debug toolbar", 160, 120);
    runtime.poll(false);
    const bool debug = true;
    SettingsOverlay overlay({}, &debug);
    runtime.begin({0, 0, 0});
    overlay.draw(runtime);
    runtime.end();
    glFinish();
    const auto color = pixel(runtime, {98.5f, 8.5f});
    CHECK_MESSAGE((color[0] > 200 && color[1] > 200), "DEBUG button is missing its final G");
}

TEST_CASE("demo and i4sim start with every visual debug layer on") {
    Runtime runtime("Default debug views", 640, 480);
    runtime.poll(false);
    for (auto variant : {Variant::demo, Variant::i4sim}) {
        CAPTURE(static_cast<int>(variant));
        auto session = start(variant, runtime.camera);
        if (variant == Variant::i4sim) {
            // The first row is SHOWCASE; the first blueprint is the next one.
            Input down, choose;
            down.pressed[std::size_t(Key::down)] = true;
            choose.pressed[std::size_t(Key::enter)] = true;
            session->advance(0, down, runtime.camera);
            session->advance(0, choose, runtime.camera);
        }
        Input menu;
        menu.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, menu, runtime.camera);
        session->render(runtime);
        glFinish();
        const float scale = DebugMenu::scale(runtime.camera.viewport);
        const auto checkbox = [&](int row) {
            return Vec2{8, 32} + Vec2{12.5f, 29.5f + float(row) * 16} * scale;
        };
        for (int row = 1; row <= (variant == Variant::demo ? 5 : 7); ++row) {
            CAPTURE(row);
            CHECK_MESSAGE(pixel(runtime, checkbox(row))[0] > 150, "visual debug layer did not start enabled");
        }
        if (variant == Variant::demo) {
            CHECK_MESSAGE(pixel(runtime, checkbox(8))[0] > 150,
                          "Demo reciprocal avoidance did not start enabled");
            for (int row : {0, 6, 7}) {
                CAPTURE(row);
                CHECK_MESSAGE(pixel(runtime, checkbox(row))[0] < 100,
                              "Demo debug default changed camera, goals or gravity");
            }
            const auto statistics = pixel(runtime, {10.5f, runtime.camera.viewport.y - 19.5f});
            CHECK_MESSAGE((statistics[0] > 200 && statistics[1] > 200 && statistics[2] < 100),
                          "Demo statistics did not start enabled");
        }
    }
}

TEST_CASE("escape does not close a lab window") {
    Runtime runtime("Launcher integration", 160, 120);
    SDL_Event escape{};
    escape.type = SDL_KEYDOWN;
    escape.key.keysym.sym = SDLK_ESCAPE;
    REQUIRE(SDL_PushEvent(&escape) == 1);
    CHECK((runtime.poll(false) && runtime.input().hit(Key::escape)));
    auto session = start(Variant::demo, runtime.camera);
    CHECK_FALSE(session->close_on_escape());
}

TEST_CASE("settings pause zoom toggle and navigate in demo and dotio") {
    Runtime runtime("Settings integration", 160, 120);
    runtime.poll(false);
    for (auto variant : {Variant::demo, Variant::dotio}) {
        CAPTURE(static_cast<int>(variant));
        auto session = start(variant, runtime.camera);
        const float initial_zoom = runtime.camera.pixels_per_unit;
        Input input;
        input.pressed[std::size_t(Key::space)] = true;
        session->advance(0, input, runtime.camera);
        CHECK_MESSAGE(!paused(*session), "Space pauses gameplay");
        input = {};
        input.wheel = 2;
        session->advance(0, input, runtime.camera);
        CHECK_MESSAGE((variant == Variant::demo ? runtime.camera.pixels_per_unit > initial_zoom
                                                : runtime.camera.pixels_per_unit == initial_zoom),
                      "Demo debug must start enabled; Dotio debug must remain opt-in");
        input = {};
        input.pressed[std::size_t(Key::control)] = true;
        session->advance(0, input, runtime.camera);
        CHECK_MESSAGE(paused(*session), "settings did not pause");
        session->render(runtime);
        PanelLayout layout(variant == Variant::dotio ? 3 : 4, runtime.camera.viewport);
        auto click = [&](float x, float y) { session->advance(0, layout.click(x, y), runtime.camera); };
        click(56, 46);
        CHECK_MESSAGE(runtime.camera.pixels_per_unit > initial_zoom, "settings zoom failed");
        click(50, 74);
        if (variant == Variant::dotio)
            click(50, 102);
        click(92, layout.overlay.row_y(sim_row));
        click(92, layout.overlay.row_y(debug_controls_row));
        session->render(runtime);
        click(92, layout.overlay.row_y(debug_controls_row));
        click(92, layout.overlay.row_y(sim_row));
        session->render(runtime);
        click(106, layout.overlay.actions_y());
        CHECK_MESSAGE(!paused(*session), "resume did not unpause");
        input = {};
        input.wheel = 1;
        const float zoom = runtime.camera.pixels_per_unit;
        session->advance(0, input, runtime.camera);
        CHECK_MESSAGE(runtime.camera.pixels_per_unit > zoom, "debug wheel unavailable");
        input = {};
        input.clicked = input.mouse_down = true;
        input.mouse = {80, 12};
        session->advance(0, input, runtime.camera);
        session->render(runtime);
        input = {};
        input.pressed[std::size_t(Key::m)] = true;
        session->advance(0, input, runtime.camera);
        CHECK_MESSAGE(session->request() == SessionRequest::none, "M returns to the menu");
        input = {};
        input.pressed[std::size_t(Key::control)] = true;
        session->advance(0, input, runtime.camera);
        PanelLayout reopened(variant == Variant::dotio ? 3 : 4, runtime.camera.viewport);
        session->advance(0, reopened.click(50, reopened.overlay.actions_y()), runtime.camera);
        CHECK_MESSAGE(session->request() == SessionRequest::menu,
                      "settings LAUNCHER did not ask for the menu");
        session->advance(0, reopened.click(146, reopened.overlay.actions_y()), runtime.camera);
        CHECK_MESSAGE(session->request() == SessionRequest::exit, "settings EXIT did not ask for exit");
        CHECK(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("every launch creates a clean session with its own camera") {
    Runtime runtime("Launcher integration", 160, 120);
    runtime.poll(false);
    auto session = start(Variant::demo, runtime.camera);
    CHECK(session->title() == "Demo");
    Input pause;
    pause.pressed[std::size_t(Key::control)] = true;
    session->advance(0, pause, runtime.camera);
    CHECK_MESSAGE(paused(*session), "Demo session did not load its bindings");
    session->render(runtime);
    session.reset();
    session = start(Variant::dotio, runtime.camera);
    session->advance(0, {}, runtime.camera);
    CHECK_MESSAGE((session->title() == "Dotio" && !paused(*session) && runtime.camera.pixels_per_unit == 25),
                  "variant switch leaked pause or camera state");
    session->render(runtime);
    session.reset();
    session = start(Variant::demo, runtime.camera);
    CHECK_MESSAGE((!paused(*session) && runtime.camera.pixels_per_unit == 17),
                  "returning to Demo did not create a clean session");
    session.reset();
    const auto names = dotio::difficulty_names();
    for (std::size_t difficulty = 0; difficulty < names.size(); ++difficulty) {
        CAPTURE(difficulty);
        session.reset();
        session = start(Variant::dotio, runtime.camera, difficulty);
        CHECK_MESSAGE(status(*session).find(names[difficulty]) != std::string::npos,
                      "the launcher lost the difficulty when creating Dotio");
    }
    session.reset();
    session = start(Variant::i4sim, runtime.camera);
    CHECK(session->title() == "i4sim");
    session->advance(0, {}, runtime.camera);
    session->render(runtime);
    CHECK_MESSAGE(status(*session).find("Loadscreen") != std::string::npos,
                  "i4sim skipped its blueprint screen");
    Input past_showcase, blueprint_choice;
    past_showcase.pressed[std::size_t(Key::down)] = true;
    blueprint_choice.pressed[std::size_t(Key::enter)] = true;
    session->advance(0, past_showcase, runtime.camera);
    session->advance(0, blueprint_choice, runtime.camera);
    session->render(runtime);
    CHECK_MESSAGE(paused(*session), "the factory blueprint did not start paused");
    session.reset();
    session = start(Variant::demo, runtime.camera);
    CHECK_MESSAGE((runtime.camera.pixels_per_unit == 17 && !paused(*session)),
                  "factory state leaked into Demo");
}

TEST_CASE("the i4sim blueprint screen asks for the menu on escape") {
    Runtime runtime("Launcher integration", 160, 120);
    runtime.poll(false);
    auto session = start(Variant::i4sim, runtime.camera);
    session->advance(0, {}, runtime.camera);
    Input escape;
    escape.pressed[std::size_t(Key::escape)] = true;
    session->advance(0, escape, runtime.camera);
    CHECK(session->request() == SessionRequest::menu);
}

TEST_CASE("dotio debug layers and ghost camera work through the debug menu") {
    Runtime runtime("Debug integration", 160, 120);
    runtime.poll(false);
    const float menu_scale = DebugMenu::scale(runtime.camera.viewport);
    {
        auto session = start(Variant::dotio, runtime.camera);
        session->advance(0, {}, runtime.camera);
        // Turn on Debug Mode through the settings panel, then close it.
        Input settings;
        settings.pressed[std::size_t(Key::control)] = true;
        session->advance(0, settings, runtime.camera);
        PanelLayout layout(3, runtime.camera.viewport);
        session->advance(0, layout.click(50, 102), runtime.camera);
        session->advance(0, settings, runtime.camera);
        Input debug_input;
        debug_input.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, debug_input, runtime.camera);
        debug_input = {};
        debug_input.clicked = true;
        debug_input.mouse_down = true;
        debug_input.mouse.x = 8 + 10 * menu_scale;
        // Exercise every debug layer and ensure the panel consumes held shooting.
        for (int row = 0; row < 11; ++row) {
            debug_input.mouse.y = 32 + (28 + float(row) * 16) * menu_scale;
            session->advance(0, debug_input, runtime.camera);
        }
        session->render(runtime);
        CHECK(glGetError() == GL_NO_ERROR);
    }
    auto session = start(Variant::demo, runtime.camera);
    session->advance(0, {}, runtime.camera);
    Input demo_debug;
    demo_debug.pressed[std::size_t(Key::backquote)] = true;
    session->advance(0, demo_debug, runtime.camera);
    demo_debug = {};
    demo_debug.clicked = true;
    demo_debug.mouse = {8 + 10 * menu_scale, 32 + 28 * menu_scale};
    session->advance(0, demo_debug, runtime.camera);
    Input pan;
    pan.held[std::size_t(Key::right)] = true;
    session->advance(0.05, pan, runtime.camera);
    CHECK_MESSAGE(
        (runtime.camera.position.x > 0 && status(*session).find("camera FREE") != std::string::npos),
        "Demo debug Ghost did not route arrow input to the camera");
    demo_debug = {};
    demo_debug.pressed[std::size_t(Key::backquote)] = true;
    session->advance(0, demo_debug, runtime.camera);
    Input teleport;
    teleport.clicked = true;
    teleport.mouse = runtime.camera.viewport / 2;
    session->advance(0, teleport, runtime.camera);
    CHECK_MESSAGE(status(*session).find("teleported") != std::string::npos,
                  "Demo Ghost click failed to teleport into free space");
    session->render(runtime);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("lab bindings keep m and escape inside the running session") {
    Runtime runtime("Launcher bindings", 160, 120);
    struct LoopSession final : lab::Session {
        int &updates, &destroyed;
        LoopSession(int &u, int &d) : updates(u), destroyed(d) {}
        ~LoopSession() override { ++destroyed; }
        void advance(double elapsed, const Input &input, Camera2D &) override {
            if (updates == 0)
                CHECK_MESSAGE((elapsed == 0 && !input.hit(Key::enter)),
                              "launcher selection leaked into the session");
            ++updates;
        }
        void render(Runtime &r) override {
            r.begin();
            r.end();
            SDL_Event event{};
            event.type = SDL_KEYDOWN;
            event.key.keysym.sym = updates % 2 ? SDLK_m : SDLK_ESCAPE;
            REQUIRE(SDL_PushEvent(&event) == 1);
        }
        std::string title() override { return "LOOP TEST"; }
        std::string status() override { return {}; }
    };
    int updates = 0, destroyed = 0;
    SDL_Event enter{};
    enter.type = SDL_KEYDOWN;
    enter.key.keysym.sym = SDLK_RETURN;
    REQUIRE(SDL_PushEvent(&enter) == 1);
    const int frames = run_launcher(
        runtime,
        {{{"REGISTERED", "TEST", {}, 0},
          [&](Camera2D &, std::size_t) { return std::make_unique<LoopSession>(updates, destroyed); }}},
        std::nullopt, 4, launcher::bindings);
    CHECK(frames == 4);
    CHECK_MESSAGE(updates == 4, "M or Escape left the session");
    CHECK(destroyed == 1);
}
