#pragma once
// Scripted driving of one Lab session for a capture: frames with fixed time steps, clicks on
// the overlays' rows, and frames written as PNG files.
#include "../../../launcher/include/applications.h"

#include <kinetra/engine.h>
#include <kinetra/session.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace dotio {
class Application;
}

namespace capture {
/// Seed of every captured session, so a refresh shows the same worlds.
inline constexpr std::uint32_t capture_seed = 42;
/// Tick rate of a captured i4sim blueprint: the session's default, which the showcase
/// measurement (showcase.h) shares.
inline constexpr unsigned i4sim_updates_per_second = 200;

/// Where SettingsOverlay::update_debug places the debug menu, in logical window pixels
/// (Kinetra's settings_view.cpp; the graphics tests use the same point).
inline constexpr kinetra::Vec2 debug_menu_origin{8, 32};

/// Rows of the Lab settings panel, in the order of lab::settings_rows.
enum class SettingsRow : std::size_t {
    statistics,
    debug_mode,
    frame_times,
    frame_limit,
    sim_controls,
    debug_controls
};

/// Drives one session in a Runtime and writes what it draws.
///
/// Every step advances the session by `step_seconds` with a scripted kinetra::Input, the
/// same call the launcher makes once per frame, so a capture shows the real session. The
/// pointer stays where the last click or point_at() put it; at the start it rests in the
/// bottom-right corner, away from every menu. Not copyable; borrows the Runtime, which must
/// outlive the stage.
class Stage {
  public:
    /// Time one step advances a session, in seconds: a 60 Hz display, so the rate counters
    /// in the statistics read as in the real application.
    static constexpr double step_seconds = 1.0 / 60;
    /// Steps per clip frame.
    static constexpr int steps_per_frame = 4;
    /// Frames per second a clip plays at: one clip frame per steps_per_frame steps.
    static constexpr int clip_fps = static_cast<int>(1 / (step_seconds * steps_per_frame) + 0.5);
    /// Scripted input for clip frame `frame` (counted from 0). Its key presses, clicks and
    /// wheel apply to the frame's first step only; held keys and buttons to all its steps.
    using FrameInput = std::function<kinetra::Input(int frame)>;

    /// A stage for scene `scene`, writing into the directory `output`, which must exist.
    Stage(kinetra::Runtime &runtime, std::filesystem::path output, std::string scene);
    Stage(const Stage &) = delete;
    Stage &operator=(const Stage &) = delete;

    /// Starts `variant` exactly as the bundled launcher does, with capture_seed, the default
    /// Demo agent count and the default Dotio difficulty. i4sim starts on its blueprint screen.
    void start(launcher::Variant variant);
    /// Starts Dotio as the bundled launcher does, at the default difficulty with capture_seed,
    /// and keeps its application for dotio(), so a script can play against the world.
    void start_dotio();
    /// The application of the session start_dotio() started, for reading between steps; valid
    /// until the next start. Throws std::logic_error when the running session is not such one.
    dotio::Application &dotio();
    /// Starts i4sim with the blueprint file `blueprint` from the bundled catalog, paused, at
    /// i4sim_updates_per_second. `blueprint` is relative to the catalog directory.
    void start_i4sim(std::string_view blueprint);

    /// Advances the session one step with `input`; its pointer is replaced by the stage's.
    void step(kinetra::Input input = {});
    /// Advances `count` steps without input or drawing.
    void steps(int count);
    /// Advances `seconds` of steps without input or drawing.
    void wait(double seconds);
    /// Advances `frames` clip frames with `input`, without drawing.
    void play(int frames, const FrameInput &input);
    /// Advances clip frames with `input`, without drawing, until `done()` holds or `limit`
    /// frames have passed; `done` is asked before every frame. Returns the frames advanced,
    /// so a recording can continue the count.
    int play_until(int limit, const FrameInput &input, const std::function<bool()> &done);
    /// One step with `key` pressed and held.
    void press(kinetra::Key key);
    /// One step with Shift+Ctrl, the debug shortcut: it shows the debug menu and switches
    /// Debug Mode on when it was off, or hides a showing menu.
    void toggle_debug_menu();
    /// Moves the pointer to `point` (logical window pixels) and clicks there.
    void click(kinetra::Vec2 point);
    /// Moves the pointer to `point` and advances one step with it there.
    void point_at(kinetra::Vec2 point);
    /// Moves the pointer to `point` for the following steps, without advancing; for a
    /// FrameInput that aims or clicks somewhere else.
    void aim(kinetra::Vec2 point) { pointer_ = point; }
    /// Opens the settings panel with Ctrl, clicks `row`, and closes it again. Requires that
    /// no help section is expanded, because the clicks use the collapsed layout.
    void toggle_setting(SettingsRow row);
    /// Opens the settings panel with a tap of Ctrl, or closes an open one, and leaves it so.
    void open_settings();
    /// Clicks `row` of the open settings panel; same layout requirement as toggle_setting().
    void click_setting(SettingsRow row);
    /// Clicks the label of debug-menu row `row` (0 is the first row below the heading).
    void click_debug_row(std::size_t row);

    /// Draws the session and writes `<output>/<scene>.png`.
    void shot();
    /// Records a clip: frame `i` advances with `input(i)`, draws and is written as
    /// `<output>/<scene>/frame-NNNN.png`, counted from 1. Also writes
    /// `<output>/<scene>/framerate`, holding clip_fps, for the GIF encoder.
    void record(int frames, const FrameInput &input);
    /// Writes the frame the Runtime presented last as `<output>/<scene>.png`, for scenes
    /// that draw without a session.
    void write_frame() const;

    /// Size of the window in logical pixels.
    kinetra::Vec2 viewport() const { return runtime_.camera.viewport; }
    /// The Runtime, for scenes that draw without a session.
    kinetra::Runtime &runtime() { return runtime_; }

  private:
    kinetra::Session &session();
    /// Advances one clip frame: `input` on the first step, its held part on the others.
    void advance_frame(const kinetra::Input &input);
    /// Screen position of the middle of settings row `row`, in the collapsed panel.
    kinetra::Vec2 settings_point(SettingsRow row) const;
    void save(const std::filesystem::path &path) const;

    kinetra::Runtime &runtime_;
    std::filesystem::path output_;
    std::string scene_;
    std::unique_ptr<kinetra::Session> session_;
    /// The application inside session_ after start_dotio(); null for every other session.
    dotio::Application *dotio_{};
    kinetra::Vec2 pointer_;
};

/// The applications' menu entries as the bundled launcher builds them, for the launcher
/// pages.
std::vector<kinetra::LaunchChoice> launcher_choices();
} // namespace capture
