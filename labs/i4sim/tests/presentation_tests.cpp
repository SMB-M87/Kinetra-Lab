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
        Input debug_menu;
        debug_menu.pressed[std::size_t(Key::backquote)] = true;
        session->advance(0, debug_menu, runtime.camera);
        auto click_row = [&](unsigned row) {
            Input click;
            click.clicked = true;
            click.mouse = Vec2{8, 32} + Vec2{12, 30 + row * 16.f} * DebugMenu::scale(runtime.camera.viewport);
            session->advance(0, click, runtime.camera);
        };
        click_row(0); // Motion page.
        auto statistics_pixels = [&] {
            session->render(runtime);
            glFinish();
            std::vector<unsigned char> pixels(640 * 40 * 3);
            glReadBuffer(GL_FRONT);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, 640, 40, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            return pixels;
        };
        const auto before_motion = statistics_pixels();
        click_row(4); // Speed + must apply while running.
        require(statistics_pixels() != before_motion, "Running Motion edit did not update statistics");
        click_row(5); // Speed - restores the initial speed.
        require(statistics_pixels() == before_motion, "Inverse Motion edit did not restore speed");
        for (int cycle = 0; cycle < 3; ++cycle)
            click_row(1);
        require(statistics_pixels() != before_motion, "Reciprocal profile did not expose avoidance limits");
        click_row(1);
        require(statistics_pixels() == before_motion, "Profile cycle did not restore Improved");
        click_row(13); // Parking traffic is appended without moving existing controls.
        require(statistics_pixels() != before_motion, "Parking traffic did not expose wait statistics");
        click_row(13);
        require(statistics_pixels() == before_motion, "Parking traffic did not restore disabled statistics");
        click_row(14);
        require(statistics_pixels() != before_motion, "Junction traffic did not expose wait statistics");
        click_row(14);
        require(statistics_pixels() == before_motion, "Junction traffic did not clear disabled statistics");
        click_row(0); // View page: corridor admission sits with scene/workload controls.
        click_row(13);
        require(statistics_pixels() != before_motion, "Corridor traffic did not expose wait statistics");
        click_row(13);
        require(statistics_pixels() == before_motion, "Corridor traffic did not clear disabled statistics");
        click_row(14); // Predictive passing is an explicit scene policy.
        require(statistics_pixels() != before_motion, "Passing toggle did not update statistics");
        click_row(14);
        require(statistics_pixels() == before_motion, "Passing toggle did not restore disabled statistics");
        click_row(0);
        for (unsigned row = 1; row <= 14; ++row) {
            click_row(row);
            require(session->title().find("RUNNING") != std::string::npos,
                    "Motion edit did not preserve running state");
        }
        session->advance(0, run, runtime.camera);
        click_row(1);
        require(session->title().find("PAUSED") != std::string::npos,
                "Motion edit resumed a paused simulation");
        session->advance(0, run, runtime.camera);
        session->advance(0, debug_menu, runtime.camera);
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
        require(session->title().find("PAUSED") != std::string::npos,
                "Modal settings leaked simulation input");
        session->advance(0, panel, runtime.camera);
        require(session->title().find("RUNNING") != std::string::npos,
                "Settings did not restore running state");
        Input back;
        back.pressed[std::size_t(Key::control)] = true;
        session->advance(0, back, runtime.camera);
        session->render(runtime);
        back = {};
        // Centered modal settings, default collapsed help: launcher action.
        const float scale =
            std::min({2.f, runtime.camera.viewport.x / 204.f, runtime.camera.viewport.y / 418.f});
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
        const auto crossing_file = std::filesystem::path(__FILE__).parent_path() / "junction-crossing.json";
        session = i4sim::create_session(42, repaired_catalog, crossing_file);
        session->render(runtime);
        session->advance(0, debug_menu, runtime.camera);
        click_row(0);
        for (int cycle = 0; cycle < 3; ++cycle)
            click_row(1);
        click_row(14);
        session->advance(0, debug_menu, runtime.camera);
        Input no_workload;
        no_workload.pressed[std::size_t(Key::g)] = true;
        session->advance(0, no_workload, runtime.camera);
        for (int i = 0; i < 2; ++i) {
            Input select;
            select.clicked = true;
            select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{300, -700} : Vec2{700, -300});
            session->advance(0, select, runtime.camera);
            select.held[std::size_t(Key::shift)] = true;
            select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1100, -700} : Vec2{700, -1100});
            session->advance(0, select, runtime.camera);
        }
        session->advance(0, run, runtime.camera);
        for (int frame = 0; frame < 60; ++frame) {
            session->advance(0.02, {}, runtime.camera);
            session->render(runtime);
        }
        glFinish();
        std::vector<unsigned char> crossing_pixels(640 * 480 * 3);
        glReadBuffer(GL_FRONT);
        glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, crossing_pixels.data());
        unsigned cyan = 0, lime = 0;
        for (std::size_t i = 0; i < crossing_pixels.size(); i += 3) {
            if (crossing_pixels[i] < 30 && crossing_pixels[i + 1] > 180 && crossing_pixels[i + 2] > 240)
                ++cyan;
            if (crossing_pixels[i] > 120 && crossing_pixels[i] < 180 && crossing_pixels[i + 1] > 235 &&
                crossing_pixels[i + 2] < 30)
                ++lime;
        }
        require(lime > 30 && cyan > 100 && session->title().find("RUNNING") != std::string::npos &&
                    glGetError() == GL_NO_ERROR,
                "Actual crossing/zone rendering failed");
        const auto corridor_file = std::filesystem::path(__FILE__).parent_path() / "corridor-passing.json";
        session = i4sim::create_session(42, repaired_catalog, corridor_file);
        session->render(runtime);
        session->advance(0, debug_menu, runtime.camera);
        click_row(0);
        for (int cycle = 0; cycle < 3; ++cycle)
            click_row(1);
        click_row(0);
        click_row(13);
        session->advance(0, debug_menu, runtime.camera);
        session->advance(0, no_workload, runtime.camera);
        for (int i = 0; i < 2; ++i) {
            Input select;
            select.clicked = true;
            select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{300, -700} : Vec2{1900, -700});
            session->advance(0, select, runtime.camera);
            select.held[std::size_t(Key::shift)] = true;
            select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1900, -700} : Vec2{300, -700});
            session->advance(0, select, runtime.camera);
        }
        session->advance(0, run, runtime.camera);
        for (int frame = 0; frame < 60; ++frame) {
            session->advance(0.02, {}, runtime.camera);
            session->render(runtime);
        }
        glFinish();
        glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, crossing_pixels.data());
        unsigned blue = 0;
        lime = 0;
        for (std::size_t i = 0; i < crossing_pixels.size(); i += 3) {
            if (crossing_pixels[i] > 30 && crossing_pixels[i] < 80 && crossing_pixels[i + 1] > 90 &&
                crossing_pixels[i + 1] < 160 && crossing_pixels[i + 2] > 240)
                ++blue;
            if (crossing_pixels[i] > 120 && crossing_pixels[i] < 180 && crossing_pixels[i + 1] > 235 &&
                crossing_pixels[i + 2] < 30)
                ++lime;
        }
        require(blue > 50 && lime > 30 && glGetError() == GL_NO_ERROR,
                "Actual corridor admission/exit rendering failed");
        const auto recovery_file = std::filesystem::path(__FILE__).parent_path() / "corridor-recovery.json";
        session = i4sim::create_session(42, repaired_catalog, recovery_file);
        session->render(runtime);
        session->advance(0, debug_menu, runtime.camera);
        click_row(0);
        for (int cycle = 0; cycle < 3; ++cycle)
            click_row(1);
        click_row(0);
        click_row(13);
        session->advance(0, debug_menu, runtime.camera);
        session->advance(0, no_workload, runtime.camera);
        for (int i = 0; i < 2; ++i) {
            Input select;
            select.clicked = true;
            select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1000, -700} : Vec2{1200, -700});
            session->advance(0, select, runtime.camera);
            select.held[std::size_t(Key::shift)] = true;
            select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1900, -700} : Vec2{300, -700});
            session->advance(0, select, runtime.camera);
        }
        session->advance(0, run, runtime.camera);
        for (int frame = 0; frame < 110; ++frame) {
            session->advance(0.02, {}, runtime.camera);
            session->render(runtime);
        }
        glFinish();
        glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, crossing_pixels.data());
        unsigned retreat_pixels = 0;
        for (std::size_t i = 0; i < crossing_pixels.size(); i += 3)
            if (crossing_pixels[i] > 230 && crossing_pixels[i + 1] > 50 && crossing_pixels[i + 1] < 100 &&
                crossing_pixels[i + 2] > 150 && crossing_pixels[i + 2] < 210)
                ++retreat_pixels;
        require(retreat_pixels > 20 && glGetError() == GL_NO_ERROR,
                "Actual corridor retreat rendering failed");
        const auto passing_file = std::filesystem::path(__FILE__).parent_path() / "predictive-passing.json";
        session = i4sim::create_session(42, repaired_catalog, passing_file);
        session->render(runtime);
        session->advance(0, debug_menu, runtime.camera);
        click_row(0);
        for (int cycle = 0; cycle < 3; ++cycle)
            click_row(1);
        click_row(2); // Plain A* keeps this controlled encounter head-on.
        click_row(0);
        click_row(14);
        session->advance(0, debug_menu, runtime.camera);
        session->advance(0, no_workload, runtime.camera);
        for (int i = 0; i < 2; ++i) {
            Input select;
            select.clicked = true;
            select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{500, -800} : Vec2{1900, -800});
            session->advance(0, select, runtime.camera);
            select.held[std::size_t(Key::shift)] = true;
            select.mouse = runtime.camera.world_to_screen(i == 0 ? Vec2{1900, -800} : Vec2{500, -800});
            session->advance(0, select, runtime.camera);
        }
        session->advance(0, run, runtime.camera);
        unsigned passing_pixels = 0;
        for (int frame = 0; frame < 180 && passing_pixels <= 20; ++frame) {
            session->advance(0.02, {}, runtime.camera);
            session->render(runtime);
            glFinish();
            glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, crossing_pixels.data());
            passing_pixels = 0;
            for (std::size_t i = 0; i < crossing_pixels.size(); i += 3)
                if (crossing_pixels[i] > 60 && crossing_pixels[i] < 100 && crossing_pixels[i + 1] > 240 &&
                    crossing_pixels[i + 2] > 160 && crossing_pixels[i + 2] < 200)
                    ++passing_pixels;
        }
        require(passing_pixels > 20 && glGetError() == GL_NO_ERROR,
                "Actual predictive passing rendering failed");
        std::cout << "PASS loadscreen, simulation overlays, input and session lifecycle\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
