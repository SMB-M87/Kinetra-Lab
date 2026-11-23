#pragma once
// The tour: the launcher and its sessions played by a scripted user, written as video
// frames with a visible pointer, click rings, keycaps and captions.
#include "capture/stage.h"
#include "capture/tour/canvas.h"
#include "capture/tour/pointer_path.h"

#include <kinetra/runtime/engine.h>
#include <kinetra/runtime/launcher.h>
#include <kinetra/runtime/settings_view.h>
#include <kinetra_lab/session.h>

#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace demo {
class Application;
}

namespace capture {
/// Receives every frame of the tour, in order.
using FrameSink = std::function<void(const Canvas &)>;

/// Size and rate of the tour video.
inline constexpr int tour_width = 1920, tour_height = 1080, tour_frames_per_second = 60;

/// Index of the row with `label` in `rows`. Throws std::runtime_error that lists the labels
/// present when there is none: the menu changed, or the wrong section is open.
std::size_t row_with_label(std::span<const kinetra_lab::DebugRow> rows, std::string_view label);

/// A scripted user in front of the bundled launcher.
///
/// The tour owns the launcher's session host and menu and drives them as
/// kinetra::run_launcher does, one frame of 1/60 s at a time, with the input a person would
/// give: a pointer that travels (PointerPath), buttons and keys that go down and up again.
/// After every frame it reads the picture back, draws the pointer, click rings, the keys
/// that are down and the caption on it, and hands it to the sink.
///
/// Positions are logical window pixels. Every verb returns when it is done and has emitted
/// the frames it took. Not copyable; borrows the Runtime, which must outlive the tour and
/// have the size of the video.
class Tour {
  public:
    /// `seed` seeds the pointer's bends and the pauses between actions.
    Tour(kinetra::Runtime &runtime, FrameSink sink, std::uint32_t seed = capture_seed);
    Tour(const Tour &) = delete;
    Tour &operator=(const Tour &) = delete;

    // Time.

    /// Plays one frame with the keys and buttons that are down.
    void frame();
    /// Plays `seconds` of frames.
    void wait(double seconds);
    /// Plays a pause as a person leaves between two actions: between `shortest` and
    /// `longest` seconds, drawn from the tour's random source.
    void pause(double shortest = 0.25, double longest = 0.5);
    /// Plays `seconds` of frames and calls `before(frame)` ahead of each, counted from 0, so
    /// a script can steer by what it reads from the session: set_held(), aim_at(), press_mouse().
    void play(double seconds, const std::function<void(int frame)> &before);
    /// Fades the picture in from black, or out to black, over `seconds` of frames.
    void fade_in(double seconds);
    void fade_out(double seconds);

    // Pointer.

    kinetra::Vec2 pointer() const { return pointer_; }
    /// Moves the pointer to `point` along a planned path.
    void move_to(kinetra::Vec2 point);
    /// Moves the pointer a part of the way to `point` for the coming frame: the fraction a
    /// hand that tracks a moving target covers in one frame. For play().
    void aim_at(kinetra::Vec2 point);
    /// Presses or releases the left button for the coming frames. For play().
    void press_mouse(bool down);
    /// Presses the left button where the pointer is, holds it a moment and releases it.
    void click();
    /// The same with the right button.
    void right_click();
    /// Moves to `point`, rests a moment and clicks.
    void click_at(kinetra::Vec2 point);
    /// Follows the point `target()` returns, which may move, until the pointer is on it, and
    /// clicks. After `longest` seconds it finishes with one planned move to where the target
    /// then is.
    void click_on(const std::function<kinetra::Vec2()> &target, double longest = 3);
    /// Turns the wheel `notches` notches (negative toward the user), one every few frames.
    void scroll(int notches);

    // Keys.

    /// Presses `keys` together, holds them for `seconds` and releases them.
    void hold(std::initializer_list<kinetra::Key> keys, double seconds);
    /// A short press of `keys` together.
    void tap(std::initializer_list<kinetra::Key> keys) { hold(keys, 0.14); }
    /// Replaces the set of keys that are down for the coming frames. For play().
    void set_held(std::initializer_list<kinetra::Key> keys);

    // What the viewer reads.

    /// Shows `text` at the bottom of the picture until the next caption; empty hides it.
    void caption(std::string text);
    /// Shows `text` large in the middle of the picture for `seconds`, while frames go on.
    void title(std::string text, double seconds);

    // The launcher and its sessions.

    /// Whether the launcher menu shows instead of a session.
    bool in_menu() const { return host_.session() == nullptr; }
    /// Whether EXIT was chosen, in the menu or in a settings panel; frames then repeat the
    /// last picture.
    bool finished() const { return finished_; }
    /// The running session. Throws std::logic_error in the menu.
    kinetra_lab::Session &session();
    kinetra::Vec2 viewport() const { return runtime_.camera.viewport; }
    const kinetra::Camera2D &camera() const { return runtime_.camera; }
    /// Clicks row `row` of a kinetra::LaunchMenu page that lists `rows` rows: the launcher's
    /// own menu, its configuration page, or i4sim's blueprint screen.
    void click_menu_page_row(std::size_t row, std::size_t rows);
    /// Clicks EXIT below a menu page that lists `rows` rows.
    void click_menu_page_exit(std::size_t rows);

    // The debug menu of the running session.

    /// Shows or hides the debug menu with Shift+Ctrl.
    void toggle_debug_menu();
    /// Clicks the label of the row `label`, found through kinetra_lab::Session::debug_rows().
    void click_row(std::string_view label);
    /// Clicks the `[+]` button of the row `label` `times` times, or with `up` false its `[-]`.
    void step_row(std::string_view label, bool up, int times = 1);

    // The settings panel of the running session.

    /// Opens the settings panel with Ctrl. `sim_lines` and `debug_lines` are the numbers of
    /// help lines of the session's SIM CONTROLS and DEBUG CONTROLS sections, which decide
    /// where the rows and buttons below an open section are.
    void open_settings(std::size_t sim_lines, std::size_t debug_lines);
    /// Clicks `row` of the open settings panel.
    void click_setting(SettingsRow row);
    /// Clicks the action button of the open panel whose center is at panel x `x`:
    /// SettingsOverlay::launcher_x, resume_x or exit_x. The panel is closed afterwards.
    void click_settings_action(float x);

  private:
    /// A ring that spreads from a click.
    struct Ring {
        kinetra::Vec2 center;
        double age{};
        kinetra::Color color;
    };
    /// A settings panel with the rows of the session's, which takes the same clicks and so
    /// knows where the rows are.
    struct PanelMirror;

    static constexpr std::size_t key_count = static_cast<std::size_t>(kinetra::Key::count);

    void step(const kinetra::Input &input);
    void read_frame();
    void draw_overlays();
    void press_button(bool right);
    kinetra::Vec2 row_point(std::size_t row, float panel_x) const;

    kinetra::Runtime &runtime_;
    FrameSink sink_;
    kinetra::SessionHost host_;
    kinetra::LaunchMenu menu_;
    std::mt19937 random_;
    bool finished_{};

    /// What is down now, and what went down or turned since the last frame.
    std::array<bool, key_count> held_{}, pressed_{};
    kinetra::Vec2 pointer_;
    bool mouse_down_{}, clicked_{}, right_down_{}, right_clicked_{};
    float wheel_{};

    /// The picture as it was read back, and the frame with the overlays on it.
    Canvas scene_, canvas_;
    std::string caption_, shown_caption_, title_;
    double caption_strength_{}, title_remaining_{}, title_strength_{};
    /// How bright each key's cap is: 1 while it is down, fading after its release.
    std::array<float, key_count> key_glow_{};
    /// The wheel's cap, and which way it last turned.
    float wheel_glow_{};
    bool wheel_up_{};
    std::vector<Ring> rings_;
    /// The frame as OpenGL returns it, bottom row first; kept so a frame needs no allocation.
    std::vector<std::uint8_t> read_back_;
    float brightness_{1};
    std::shared_ptr<PanelMirror> panel_;
};

/// The Demo application of the tour's running session. Throws std::bad_cast when another
/// application runs, and std::logic_error in the launcher menu.
const demo::Application &demo_application(Tour &tour);
/// Screen position of the awake Demo agent nearest to screen point `near`, or `near` itself
/// when no agent is awake. Same requirements as demo_application().
kinetra::Vec2 nearest_demo_agent(Tour &tour, kinetra::Vec2 near);

/// Throws std::runtime_error unless `running`, the session the launcher runs or nullptr while
/// one of its menu pages shows, is Demo on its Terrain scenario. The message starts with
/// "The tour did not start Demo Terrain" and says what runs instead: a menu page, such as
/// Demo's scenario page, another application by its title(), or another Demo scenario by
/// its label.
void require_demo_terrain(kinetra_lab::Session *running);

/// Plays the whole tour. After the launcher chapter, throws std::runtime_error when Demo
/// Terrain did not start (require_demo_terrain), and calls `terrain_started` when it did.
void play_tour(Tour &tour, const std::function<void()> &terrain_started);
} // namespace capture
