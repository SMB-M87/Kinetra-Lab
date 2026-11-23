// Graphics tests for the Demo inspector, routes and steering probes.
#include "demo/presentation/renderer.h"
#include "demo/presentation/session.h"
#include "demo/zoom.h"

#include <kinetra/runtime/engine.h>

#include <SDL2/SDL.h>
#include <doctest/doctest.h>
#include <glad/glad.h>

#include <algorithm>
#include <string>

using namespace kinetra;

TEST_CASE("Demo inspector coexists with group goals, Ghost teleport and debug pause") {
    Runtime runtime("Demo inspector", 1280, 800);
    auto session = demo::create_session(6, 42);
    auto &camera = runtime.camera;
    camera.pixels_per_unit = demo::initial_zoom;
    session->advance(0, {}, camera);
    const auto &application = session->application();
    const auto &scene = application.scene();
    Input menu;
    menu.pressed[std::size_t(Key::shift)] = menu.pressed[std::size_t(Key::control)] = true;
    session->advance(0, menu, camera);
    const auto click = [&](Vec2 point) {
        Input input;
        input.clicked = true;
        input.mouse = point;
        session->advance(0, input, camera);
    };
    const auto row = [&](std::string_view label) {
        const auto rows = session->debug_rows();
        const auto found = std::ranges::find(rows, label, &kinetra_lab::DebugRow::label);
        REQUIRE(found != rows.end());
        click(Vec2{8, 32} + Vec2{20, DebugMenu::row_top(std::size_t(found - rows.begin())) + 8} *
                                DebugMenu::scale(camera.viewport));
    };
    const auto agents = scene.agents();
    const auto active = std::ranges::find_if(agents, [&](const auto &agent) {
        const auto position = camera.world_to_screen(scene.world().body(agent.body_id()).position);
        return agent.active() && position.x > 350 && position.x < 900 && position.y > 300 && position.y < 700;
    });
    REQUIRE(active != agents.end());
    click(camera.world_to_screen(scene.world().body(active->body_id()).position));
    REQUIRE(scene.selected_agent());
    row("AGENTS");
    row("CLICK SETS GOAL");
    click(camera.world_to_screen({0, 0}));
    CHECK(application.last_action() == demo::ActionResult::agent_goals_set);
    CHECK(scene.selected_agent());
    row("GHOST CAMERA");
    click(camera.world_to_screen({0, 0}));
    REQUIRE(application.last_action() == demo::ActionResult::teleported);
    REQUIRE(scene.selected_agent());
    const auto player = scene.world().body(scene.player()).position;
    row("SIMULATION");
    row("RUNNING");
    CHECK(application.paused());
    click({1260, 20});
    CHECK_FALSE(scene.selected_agent());
    CHECK(length(scene.world().body(scene.player()).position - player) == 0);
    CHECK(application.last_action() == demo::ActionResult::teleported);
}

TEST_CASE("routes and steering probes render at different camera zoom levels") {
    Runtime runtime("Demo graphics", 160, 120);
    runtime.camera.pixels_per_unit = 73;
    demo::Application application(0, 42);
    application.advance(0, {}, runtime.camera);
    demo::render(runtime, application);
    demo::FrameCommands debug;
    debug.toggle_paths = debug.toggle_vectors = true;
    debug.pointer = demo::PointerCommand{demo::PointerAction::walk, runtime.camera.world_to_screen({0, -17})};
    application.advance(0, debug, runtime.camera);
    REQUIRE_MESSAGE(application.last_action() == demo::ActionResult::walking,
                    "Missing route for renderer check");
    demo::render(runtime, application);
    CHECK_MESSAGE(glGetError() == GL_NO_ERROR, "Scene renderer reported an OpenGL error");
    demo::FrameCommands probes;
    probes.probes = true;
    probes.toggle_paths = probes.toggle_vectors = true;
    application.advance(0, probes, runtime.camera);
    const auto &player = application.scene().world().body(application.scene().player());
    for (float zoom : {5.f, 73.f}) {
        CAPTURE(zoom);
        runtime.camera.pixels_per_unit = zoom;
        runtime.camera.position = player.position + rotated(Vec2{3, 0}, player.angle);
        demo::render(runtime, application);
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
