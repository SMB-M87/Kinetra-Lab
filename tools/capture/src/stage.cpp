#include "capture/stage.h"

#include "capture/png.h"
#include "demo/presentation/session.h"
#include "dotio/presentation/session.h"
#include "dotio/zoom.h"
#include "i4sim/presentation/session.h"

#include "kinetra_lab/settings.h"
#include <glad/glad.h>
#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/launcher.h>
#include <kinetra/runtime/settings_view.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace capture {
namespace {
using kinetra::Input;
using kinetra::Key;
using kinetra::Vec2;

/// Where SettingsOverlay::update_debug places the debug menu, in logical window pixels
/// (Kinetra's settings_view.cpp; the graphics tests use the same point).
constexpr Vec2 debug_menu_origin{8, 32};
/// Panel x of a click on a settings row: the middle of the panel, inside its click column.
constexpr float settings_click_x = kinetra::SettingsOverlay::panel_width / 2;
/// Panel y of a click below a settings row's text top, inside its click band.
constexpr float settings_click_below_top = 5;
/// Panel x of a click on a debug-menu row: on the label of a row at the deepest level, and
/// left of every row's buttons.
constexpr float debug_click_x = kinetra::DebugMenu::label_left(2) + 2;

launcher::Options capture_options() {
    return {demo::default_agent_count, capture_seed, dotio::default_difficulty};
}
} // namespace

std::vector<kinetra::LaunchChoice> launcher_choices() {
    std::vector<kinetra::LaunchChoice> choices;
    for (auto &registration : launcher::applications(capture_options()))
        choices.push_back(std::move(registration.choice));
    return choices;
}

Stage::Stage(kinetra::Runtime &runtime, std::filesystem::path output, std::string scene)
    : runtime_(runtime), output_(std::move(output)), scene_(std::move(scene)),
      pointer_(runtime.camera.viewport - Vec2{2, 2}) {
}

void Stage::start(launcher::Variant variant) {
    // Release the old session first: Dotio's world is a singleton.
    session_.reset();
    dotio_ = nullptr;
    const auto options = capture_options();
    auto registrations = launcher::applications(options);
    const auto selection = launcher::selection(variant, options);
    session_ = registrations.at(selection.application).create(runtime_.camera, selection.configuration);
}

void Stage::start_dotio() {
    session_.reset();
    dotio_ = nullptr;
    // What the launcher's Dotio entry does, but keeping the session's own type.
    runtime_.camera.position = {};
    runtime_.camera.pixels_per_unit = dotio::initial_zoom;
    auto session = dotio::create_session(capture_seed, dotio::default_difficulty);
    dotio_ = &session->application();
    session_ = std::move(session);
}

dotio::Application &Stage::dotio() {
    if (!dotio_)
        throw std::logic_error("Scene " + scene_ + " asked for Dotio's application without start_dotio()");
    return *dotio_;
}

void Stage::start_i4sim(std::string_view blueprint) {
    session_.reset();
    dotio_ = nullptr;
    runtime_.camera.position = {};
    runtime_.camera.pixels_per_unit = i4sim::initial_zoom;
    const auto directory = i4sim::default_blueprint_directory();
    session_ = i4sim::create_session(capture_seed, directory, directory / std::string(blueprint), false,
                                     i4sim_updates_per_second);
}

kinetra::Session &Stage::session() {
    if (!session_)
        throw std::logic_error("Scene " + scene_ + " used its session before starting one");
    return *session_;
}

void Stage::step(Input input) {
    input.mouse = pointer_;
    session().advance(step_seconds, input, runtime_.camera);
}

void Stage::steps(int count) {
    for (int i = 0; i < count; ++i)
        step();
}

void Stage::wait(double seconds) {
    steps(static_cast<int>(seconds / step_seconds + 0.5));
}

void Stage::advance_frame(const Input &input) {
    step(input);
    // Only what stays held carries over; an edge (press, click, wheel) happens once.
    Input held = input;
    held.pressed = {};
    held.clicked = held.right_clicked = false;
    held.wheel = 0;
    for (int i = 1; i < steps_per_frame; ++i)
        step(held);
}

void Stage::play(int frames, const FrameInput &input) {
    for (int frame = 0; frame < frames; ++frame)
        advance_frame(input(frame));
}

int Stage::play_until(int limit, const FrameInput &input, const std::function<bool()> &done) {
    int frame = 0;
    while (frame < limit && !done())
        advance_frame(input(frame++));
    return frame;
}

void Stage::press(Key key) {
    Input input;
    input.pressed[std::size_t(key)] = true;
    input.held[std::size_t(key)] = true;
    step(input);
}

void Stage::click(Vec2 point) {
    pointer_ = point;
    Input input;
    input.clicked = true;
    input.mouse_down = true;
    step(input);
}

void Stage::point_at(Vec2 point) {
    pointer_ = point;
    step();
}

Vec2 Stage::settings_point(SettingsRow row) const {
    // A panel with the same Lab rows has the same layout; the flags and help lines do not
    // move collapsed rows.
    bool statistics{}, debug{}, frame_times{}, frame_limit{};
    const kinetra::SettingsOverlay mirror(
        kinetra_lab::settings_rows(statistics, debug, frame_times, frame_limit, {"LINE"}, {"LINE"}));
    const Vec2 panel{settings_click_x, mirror.row_y(std::size_t(row)) + settings_click_below_top};
    return viewport() / 2 + (panel - mirror.half_extent()) * kinetra::SettingsOverlay::scale(viewport());
}

void Stage::toggle_debug_menu() {
    Input input;
    input.pressed[std::size_t(Key::shift)] = input.pressed[std::size_t(Key::control)] = true;
    step(input);
}

void Stage::open_settings() {
    // A tap: the panel acts when Ctrl is released, and a press that is not held is complete.
    Input input;
    input.pressed[std::size_t(Key::control)] = true;
    step(input);
}

void Stage::click_setting(SettingsRow row) {
    click(settings_point(row));
}

void Stage::toggle_setting(SettingsRow row) {
    const Vec2 rest = pointer_;
    open_settings();
    click_setting(row);
    open_settings();
    pointer_ = rest;
}

void Stage::click_debug_row(std::size_t row) {
    const float scale = kinetra::DebugMenu::scale(viewport());
    click(debug_menu_origin +
          Vec2{debug_click_x, kinetra::DebugMenu::row_top(row) + kinetra::DebugMenu::row_height / 2} * scale);
}

void Stage::shot() {
    session().render(runtime_);
    write_frame();
}

void Stage::record(int frames, const FrameInput &input) {
    const auto directory = output_ / scene_;
    std::filesystem::create_directories(directory);
    // The encoder reads the rate from here, so it cannot disagree with the steps.
    std::ofstream(directory / "framerate") << clip_fps << '\n';
    for (int frame = 0; frame < frames; ++frame) {
        advance_frame(input(frame));
        session().render(runtime_);
        std::array<char, 32> name{};
        std::snprintf(name.data(), name.size(), "frame-%04d.png", frame + 1);
        save(directory / name.data());
    }
}

void Stage::write_frame() const {
    save(output_ / (scene_ + ".png"));
}

void Stage::save(const std::filesystem::path &path) const {
    // The drawable size, which equals the logical size without display scaling.
    std::array<GLint, 4> area{};
    glGetIntegerv(GL_VIEWPORT, area.data());
    const auto width = static_cast<std::uint32_t>(area[2]), height = static_cast<std::uint32_t>(area[3]);
    std::vector<std::uint8_t> pixels(std::size_t(width) * height * 3);
    // Runtime::end() presented the frame, so it is in the front buffer, as the graphics
    // tests read it.
    glReadBuffer(GL_FRONT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, area[2], area[3], GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    // OpenGL rows run bottom-up; PNG rows top-down.
    const std::size_t stride = std::size_t(width) * 3;
    std::vector<std::uint8_t> flipped(pixels.size());
    for (std::uint32_t y = 0; y < height; ++y)
        std::copy_n(pixels.data() + (height - 1 - y) * stride, stride, flipped.data() + y * stride);
    write_png(path, width, height, flipped);
}
} // namespace capture
