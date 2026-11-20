// Graphics integration tests for the bundled launcher: the real Lab sessions, created
// through launcher::applications(), with an OpenGL context. They need
// KINETRA_TEST_GRAPHICS; only one Runtime exists at a time, so every case owns its own.
#include "dotio/presentation/session.h"
#include "kinetra_lab/session.h"
#include "kinetra_lab/settings.h"
#include "launcher/applications.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/engine.h>
#include <kinetra/runtime/settings_view.h>

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
    return dynamic_cast<kinetra_lab::Session &>(session).status();
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

/// A panel with the same rows as a Lab session's, used to address its rows and buttons and
/// to mirror which help sections are open.
struct PanelLayout {
    bool statistics{}, debug{}, frame_times{}, frame_limit{};
    SettingsOverlay overlay;
    Camera2D camera;
    explicit PanelLayout(std::size_t sim_lines, Vec2 viewport)
        : overlay(kinetra_lab::settings_rows(statistics, debug, frame_times, frame_limit,
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
    /// A click on the label of row `row`.
    Input click_row(std::size_t row) {
        return click(SettingsOverlay::panel_width / 2, overlay.row_y(row) + 3);
    }
    /// A click on the action button whose center is at panel x `x`.
    Input click_action(float x) { return click(x, overlay.actions_y() + 3); }
};
constexpr std::size_t statistics_row = 0, debug_mode_row = 1, frame_limit_row = 3, sim_row = 4,
                      debug_controls_row = 5;

/// A tap of Ctrl, which opens or closes the settings panel.
Input settings_key() {
    Input input;
    input.pressed[std::size_t(Key::control)] = true;
    return input;
}

/// Shift+Ctrl, which shows or hides the debug menu and switches Debug Mode on.
Input debug_keys() {
    Input input;
    input.pressed[std::size_t(Key::shift)] = input.pressed[std::size_t(Key::control)] = true;
    return input;
}

/// Where sessions place the debug menu (SettingsOverlay::update_debug).
constexpr Vec2 debug_menu_origin{8, 32};

/// A screen point on debug-menu row `row`, at panel x `x`.
Vec2 menu_point(const Runtime &runtime, std::size_t row, float x) {
    return debug_menu_origin + Vec2{x, DebugMenu::row_top(row) + DebugMenu::row_height / 2} *
                                   DebugMenu::scale(runtime.camera.viewport);
}

/// A click on the label of debug-menu row `row`.
Input menu_click(const Runtime &runtime, std::size_t row) {
    Input input;
    input.clicked = input.mouse_down = true;
    input.mouse = menu_point(runtime, row, DebugMenu::label_left(2) + 2);
    return input;
}

/// Whether the check box of debug-menu row `row` at nesting level `depth` is drawn as on:
/// a filled box in the "on" color; a box that is off shows the panel through its middle.
bool checked(Runtime &runtime, std::size_t row, int depth) {
    const auto color =
        pixel(runtime, debug_menu_origin + Vec2{DebugMenu::label_left(depth) - DebugMenu::mark_width + 2.5f,
                                                DebugMenu::row_top(row) + DebugMenu::text_top + 3.5f} *
                                               DebugMenu::scale(runtime.camera.viewport));
    return color[1] > 180 && color[0] < 140;
}
} // namespace

TEST_CASE("the debug toolbar button shows beside settings only in debug mode") {
    Runtime runtime("Debug toolbar", 160, 120);
    runtime.poll(false);
    bool debug = true;
    SettingsOverlay overlay({}, &debug);
    // The buttons are drawn three pixels inside their cells: SETTINGS to x 64, DEBUG to 112.
    const auto button = [&](float x) {
        // Drawn twice, so both swap buffers hold the frame: which one a read returns right
        // after a swap differs between drivers, most of all for a window's first frame.
        for (int frame = 0; frame < 2; ++frame) {
            runtime.begin({0, 0, 0});
            overlay.draw(runtime);
            runtime.end();
        }
        glFinish();
        const auto color = pixel(runtime, {x, 5.5f});
        return color[2] > 40;
    };
    CHECK_MESSAGE(button(10.5f), "SETTINGS button missing");
    CHECK_MESSAGE(button(105.5f), "DEBUG button missing in debug mode");
    debug = false;
    CHECK_MESSAGE(button(10.5f), "SETTINGS button missing outside debug mode");
    CHECK_MESSAGE(!button(105.5f), "DEBUG button shows outside debug mode");
}

TEST_CASE("demo and i4sim start with every visual debug layer on") {
    // Tall enough for i4sim's LAYERS section with every part listed (25 menu rows).
    Runtime runtime("Default debug views", 640, 800);
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
        session->advance(0, debug_keys(), runtime.camera);
        // SIMULATION is open with four rows in both; the LAYERS heading on row 5 opens the layers.
        session->advance(0, menu_click(runtime, 5), runtime.camera);
        Input away;
        away.mouse = {600, 780};
        session->advance(0, away, runtime.camera);
        session->render(runtime);
        glFinish();
        if (variant == Variant::demo) {
            // LAYERS heading on row 1, then VECTORS, STEERING PROBES, PATHS, COLLISION SHAPES
            // and ACTIVE AGENTS, which start on, and HOME RANGE, which starts off.
            for (std::size_t row = 2; row <= 6; ++row) {
                CAPTURE(row);
                CHECK_MESSAGE(checked(runtime, row, 1), "visual debug layer did not start enabled");
            }
            CHECK_MESSAGE(!checked(runtime, 7, 1), "HOME RANGE started enabled");
            // Below the sections: MOTION, AGENTS, ZOOM, then GHOST CAMERA on row 11.
            CHECK_MESSAGE(!checked(runtime, 11, 0), "Demo started with the ghost camera");
            const auto statistics = pixel(runtime, {10.5f, runtime.camera.viewport.y - 19.5f});
            CHECK_MESSAGE((statistics[0] > 200 && statistics[1] > 200 && statistics[2] < 100),
                          "Demo statistics did not start enabled");
        } else {
            // LAYERS heading on row 1, then the seven layers (depth 1) with their parts (depth 2).
            for (std::size_t row = 2; row <= 19; ++row) {
                const bool part = (row >= 5 && row <= 7) || (row >= 9 && row <= 14) || row == 16 || row == 17;
                CAPTURE(row);
                CHECK_MESSAGE(checked(runtime, row, part ? 2 : 1),
                              "visual debug layer or part did not start enabled");
            }
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

TEST_CASE("settings pause toggle and navigate in demo and dotio") {
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
        session->advance(0, settings_key(), runtime.camera);
        CHECK_MESSAGE(paused(*session), "settings did not pause");
        session->render(runtime);
        PanelLayout layout(variant == Variant::dotio ? 3 : 4, runtime.camera.viewport);
        const float zoom_in_panel = runtime.camera.pixels_per_unit;
        input = {};
        input.wheel = 2;
        session->advance(0, input, runtime.camera);
        CHECK_MESSAGE(runtime.camera.pixels_per_unit == zoom_in_panel, "the wheel zoomed behind the panel");
        session->advance(0, layout.click_row(statistics_row), runtime.camera);
        if (variant == Variant::dotio)
            session->advance(0, layout.click_row(debug_mode_row), runtime.camera);
        session->advance(0, layout.click_row(sim_row), runtime.camera);
        session->advance(0, layout.click_row(debug_controls_row), runtime.camera);
        session->render(runtime);
        session->advance(0, layout.click_row(debug_controls_row), runtime.camera);
        session->advance(0, layout.click_row(sim_row), runtime.camera);
        session->render(runtime);
        session->advance(0, layout.click_action(SettingsOverlay::resume_x), runtime.camera);
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
        session->advance(0, settings_key(), runtime.camera);
        PanelLayout reopened(variant == Variant::dotio ? 3 : 4, runtime.camera.viewport);
        session->advance(0, reopened.click_action(SettingsOverlay::launcher_x), runtime.camera);
        CHECK_MESSAGE(session->request() == SessionRequest::menu,
                      "settings LAUNCHER did not ask for the menu");
        session->advance(0, reopened.click_action(SettingsOverlay::exit_x), runtime.camera);
        CHECK_MESSAGE(session->request() == SessionRequest::exit, "settings EXIT did not ask for exit");
        CHECK(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("the frame limit row follows the window and switches its limiter") {
    Runtime runtime("Frame limit", 160, 120);
    runtime.poll(false);
    auto session = start(Variant::demo, runtime.camera);
    session->advance(0, {}, runtime.camera);
    session->render(runtime);
    const auto before = runtime.pacing();
    // Switch FRAME LIMIT off in the settings panel; the session passes it on when it draws.
    session->advance(0, settings_key(), runtime.camera);
    PanelLayout layout(4, runtime.camera.viewport);
    session->advance(0, layout.click_row(frame_limit_row), runtime.camera);
    session->advance(0, settings_key(), runtime.camera);
    session->render(runtime);
    if (before == FramePacing::vsync) {
        CHECK_MESSAGE(runtime.pacing() == FramePacing::vsync, "the row has no say where VSync paces frames");
        return;
    }
    CHECK_MESSAGE(runtime.pacing() == FramePacing::uncapped, "FRAME LIMIT off left the limiter on");
    // The choice belongs to the window: the next session starts from it and can switch it back.
    session.reset();
    session = start(Variant::dotio, runtime.camera);
    session->advance(0, {}, runtime.camera);
    session->render(runtime);
    CHECK_MESSAGE(runtime.pacing() == FramePacing::uncapped, "a new session switched the limiter back on");
    session->advance(0, settings_key(), runtime.camera);
    PanelLayout next(3, runtime.camera.viewport);
    session->advance(0, next.click_row(frame_limit_row), runtime.camera);
    session->advance(0, settings_key(), runtime.camera);
    session->render(runtime);
    CHECK_MESSAGE(runtime.pacing() == FramePacing::limited, "FRAME LIMIT on did not restore the limiter");
}

TEST_CASE("every launch creates a clean session with its own camera") {
    Runtime runtime("Launcher integration", 160, 120);
    runtime.poll(false);
    auto session = start(Variant::demo, runtime.camera);
    CHECK(session->title() == "Demo");
    session->advance(0, settings_key(), runtime.camera);
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

TEST_CASE("the debug shortcut opens the menu in dotio and demo and its rows reach the application") {
    Runtime runtime("Debug integration", 640, 480);
    runtime.poll(false);
    {
        auto session = start(Variant::dotio, runtime.camera);
        session->advance(0, {}, runtime.camera);
        CHECK_MESSAGE(status(*session).find("DEBUG") == std::string::npos, "Dotio debug must be opt-in");
        // Shift+Ctrl switches Debug Mode on and shows the menu, without the settings panel.
        session->advance(0, debug_keys(), runtime.camera);
        CHECK_MESSAGE(status(*session).find("DEBUG") != std::string::npos,
                      "the debug shortcut did not switch Debug Mode on");
        CHECK_MESSAGE(!paused(*session), "the debug shortcut opened the settings panel");
        // SIMULATION is open: RUNNING on row 1 pauses and resumes.
        session->advance(0, menu_click(runtime, 1), runtime.camera);
        CHECK_MESSAGE(paused(*session), "RUNNING did not pause Dotio");
        session->advance(0, menu_click(runtime, 1), runtime.camera);
        CHECK_MESSAGE(!paused(*session), "RUNNING did not resume Dotio");
        // LAYERS on row 5 opens the layers; click each, PATHS included, which adds its parts.
        // The panel consumes the held button, so none of these clicks shoots.
        session->advance(0, menu_click(runtime, 5), runtime.camera);
        for (std::size_t row = 2; row <= 12; ++row)
            session->advance(0, menu_click(runtime, row), runtime.camera);
        session->render(runtime);
        glFinish();
        for (std::size_t row = 2; row <= 12; ++row) {
            CAPTURE(row);
            CHECK_MESSAGE(checked(runtime, row, row >= 5 && row <= 8 ? 2 : 1), "a Dotio layer row is not on");
        }
        // Below the sections: ENEMIES, SPAWNING, then ZOOM on row 15 with its [-] button.
        const float zoom = runtime.camera.pixels_per_unit;
        Input zoom_out;
        zoom_out.clicked = true;
        zoom_out.mouse = menu_point(runtime, 15, DebugMenu::decrease_left + DebugMenu::stepper_width / 2);
        session->advance(0, zoom_out, runtime.camera);
        CHECK_MESSAGE(runtime.camera.pixels_per_unit < zoom, "the ZOOM row did not zoom out");
        // GHOST CAMERA on row 16.
        session->advance(0, menu_click(runtime, 16), runtime.camera);
        CHECK_MESSAGE(status(*session).find("GHOST") != std::string::npos, "GHOST CAMERA did not detach");
        session->render(runtime);
        CHECK(glGetError() == GL_NO_ERROR);
    }
    auto session = start(Variant::demo, runtime.camera);
    session->advance(0, {}, runtime.camera);
    session->advance(0, debug_keys(), runtime.camera);
    // SIMULATION is open: RUNNING on row 1; below the sections ZOOM on row 8 and GHOST CAMERA on 9.
    session->advance(0, menu_click(runtime, 1), runtime.camera);
    CHECK_MESSAGE(paused(*session), "RUNNING did not pause the Demo");
    session->advance(0, menu_click(runtime, 1), runtime.camera);
    CHECK_MESSAGE(!paused(*session), "RUNNING did not resume the Demo");
    const float zoom = runtime.camera.pixels_per_unit;
    Input zoom_in;
    zoom_in.clicked = true;
    zoom_in.mouse = menu_point(runtime, 8, DebugMenu::increase_left + DebugMenu::stepper_width / 2);
    session->advance(0, zoom_in, runtime.camera);
    CHECK_MESSAGE(runtime.camera.pixels_per_unit > zoom, "the ZOOM row did not zoom in");
    session->advance(0, menu_click(runtime, 9), runtime.camera);
    Input pan;
    pan.held[std::size_t(Key::right)] = true;
    session->advance(0.05, pan, runtime.camera);
    CHECK_MESSAGE(
        (runtime.camera.position.x > 0 && status(*session).find("camera FREE") != std::string::npos),
        "Demo debug Ghost did not route arrow input to the camera");
    session->advance(0, debug_keys(), runtime.camera);
    Input teleport;
    teleport.clicked = true;
    teleport.mouse = runtime.camera.viewport / 2;
    session->advance(0, teleport, runtime.camera);
    CHECK_MESSAGE(status(*session).find("teleported") != std::string::npos,
                  "Demo Ghost click failed to teleport into free space");
    session->render(runtime);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("every session reports the rows of its debug menu") {
    Runtime runtime("Debug rows", 640, 480);
    runtime.poll(false);
    for (auto variant : {Variant::demo, Variant::dotio, Variant::i4sim}) {
        CAPTURE(static_cast<int>(variant));
        auto session = start(variant, runtime.camera);
        auto &lab_session = dynamic_cast<kinetra_lab::Session &>(*session);
        session->advance(0, {}, runtime.camera);
        if (variant == Variant::i4sim) {
            CHECK_MESSAGE(lab_session.debug_rows().empty(), "the blueprint screen has no debug menu");
            Input down, choose;
            down.pressed[std::size_t(Key::down)] = true;
            choose.pressed[std::size_t(Key::enter)] = true;
            session->advance(0, down, runtime.camera);
            session->advance(0, choose, runtime.camera);
        }
        // The rows are there whether the menu shows or not, with SIMULATION open at the start.
        auto rows = lab_session.debug_rows();
        REQUIRE(rows.size() > 5);
        CHECK((rows[0].label == "SIMULATION" && rows[0].depth == 0));
        CHECK((rows[1].label == "RUNNING" && rows[1].depth == 1));
        CHECK(rows[5].label == "LAYERS");
        // A click on a heading changes them: LAYERS opens and SIMULATION closes.
        session->advance(0, debug_keys(), runtime.camera);
        session->advance(0, menu_click(runtime, 5), runtime.camera);
        rows = lab_session.debug_rows();
        REQUIRE(rows.size() > 2);
        CHECK(rows[1].label == "LAYERS");
        CHECK_MESSAGE(rows[2].depth == 1, "the rows of the open section follow its heading");
    }
}

TEST_CASE("lab bindings keep m and escape inside the running session") {
    Runtime runtime("Launcher bindings", 160, 120);
    struct LoopSession final : kinetra_lab::Session {
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
