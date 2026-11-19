#pragma once
// The headless half of an i4sim session: it owns the loaded simulation and turns input
// into commands. It needs no window, so every rule of the session's behavior is testable
// from plain input values. The graphical session (src/session.cpp) adds the panels and
// the drawing.
#include "scenario.h"
#include "simulation.h"
#include "view.h"

#include <kinetra/engine.h>
#include <lab/input.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace i4sim {
/// One row of the debug menu, without the drawing types of the engine's menu widget.
///
/// An entry is a button (`action` only), a button with a value (`value` set) or a check
/// box (`checked` set; `action` flips it). The functions borrow the controller that made
/// the entry: call them only while that controller is alive, and ask for new entries after
/// the controller changed, because the page decides which entries exist.
struct MenuEntry {
    /// Row text; refers to a string literal.
    std::string_view label;
    /// Runs the command. Never empty.
    std::function<void()> action;
    /// Current value to show after the label, such as "ON" or a profile name; may be empty.
    std::function<std::string()> value{};
    /// Current state of a check box; empty for a button.
    std::function<bool()> checked{};
};

/// What the panels in front of the world did with a frame's input.
struct PanelState {
    /// The pointer is on a panel or its button, so clicks and the wheel are not for the world.
    bool pointer_captured{};
    /// Debug mode is on; only then does the wheel zoom.
    bool debug_mode{true};
};

/// Owns the simulation of the loaded blueprint and applies the operator's commands to it.
///
/// A controller starts without a blueprint. load() gives it one; the BLUEPRINTS menu entry,
/// or a scene that cannot be replayed, takes it away again. Every call that reads or
/// commands the simulation requires loaded(); without a blueprint such a call is a
/// precondition violation. One frame of a loaded blueprint is, in this order:
/// begin_frame(), suspend(), the actions of clicked menu() entries, handle_input(), and
/// update_camera() before drawing.
///
/// Positions are factory millimeters with Y down; the camera's world has Y up (flip_y).
/// Not thread-safe. Not copyable or movable, because menu entries refer to the controller.
class FactoryController {
  public:
    /// `seed` seeds every simulation the controller builds. `running` starts a loaded
    /// blueprint unpaused. `updates_per_second` is the tick rate of a loaded blueprint.
    /// Throws std::invalid_argument for a rate outside 1..10000.
    FactoryController(std::uint32_t seed, bool running, unsigned updates_per_second);
    FactoryController(const FactoryController &) = delete;
    FactoryController &operator=(const FactoryController &) = delete;

    /// Loads the blueprint at `path` with its scenario section, if it has one, and replaces
    /// the loaded blueprint. The new simulation is paused, or running when the controller
    /// was built so; the selection and the message are cleared and a camera fit is pending.
    /// On failure nothing changes except the message: report_error() gets the path and the
    /// cause, and the function returns false.
    bool load(const std::filesystem::path &path);
    /// Whether a blueprint is loaded.
    bool loaded() const { return simulation_.has_value(); }
    /// The simulation of the loaded blueprint. The reference is valid until the next
    /// load(), begin_frame() or handle_input().
    const Simulation &simulation() const { return *simulation_; }
    /// The scenario's description line; empty without a scenario or a description.
    std::string_view description() const;

    /// Which debug layers are selected.
    const ViewLayers &layers() const { return layers_; }
    /// The mover that is inspected, by index into Simulation::movers(). At most one of the
    /// mover and the producer is selected.
    std::optional<std::size_t> selected_mover() const { return selected_mover_; }
    /// The producer that is inspected, by index into Simulation::producers().
    std::optional<std::size_t> selected_producer() const { return selected_producer_; }

    /// The message for the operator; empty for none. A rejected command sets one that
    /// expires after four seconds of begin_frame() time.
    const std::string &message() const { return message_; }
    /// Shows `message` until a load, a restart or a command replaces or clears it, and
    /// writes it to standard error.
    void report_error(const std::string &message);
    /// Removes the message.
    void clear_message();

    /// Starts a frame: closes the blueprint when the BLUEPRINTS entry asked for it, and
    /// otherwise ages the message by `elapsed` seconds. Returns loaded(); after false the
    /// frame is over.
    bool begin_frame(double elapsed);
    /// Pauses the simulation while a modal panel is `open` and restores the run state it
    /// had when the panel closes (lab::ModalPause). Call once per frame.
    void suspend(bool open);
    /// The entries of the current debug-menu page, in display order: the page switch, then
    /// the view page (layers, run controls, corridor traffic, predictive passing) or the
    /// motion page (profile, planner, parameters, update rate, parking and junction traffic).
    std::vector<MenuEntry> menu();
    /// Applies a frame of input and then advances the simulation by `elapsed` seconds.
    ///
    /// Keys: Space or P pause and resume; R restarts the blueprint, paused, with the motion
    /// settings in use, and replays a scenario's goals; G toggles the workload; F fits the
    /// camera; 1 to 6 toggle grid, heatmap, paths, forces, borders and detection; Q and E
    /// halve and double the update rate within 1..10000. The camera pans with WASD or the
    /// arrows and zooms with the wheel as `panels` allows. A click selects the mover or
    /// producer under it, or clears the selection; a right click also holds or blocks it;
    /// Shift-click sends the selected mover there. Pointer commands are skipped while
    /// `panels.pointer_captured`.
    ///
    /// When a scenario cannot be replayed after R, the blueprint is closed with the cause as
    /// the message. Throws std::invalid_argument for a negative or non-finite `elapsed`.
    void handle_input(double elapsed, const kinetra::Input &input, kinetra::Camera2D &camera,
                      PanelState panels);
    /// Fits the camera to the floor when a fit is pending: after a load and after the
    /// FRAME SCENE entry.
    void update_camera(kinetra::Camera2D &camera);

  private:
    void close();
    void restart();
    void toggle_pause();
    void change_motion(const std::function<void(MotionSettings &)> &change);
    void change_update_rate(bool faster);
    void fit(kinetra::Camera2D &camera);
    void handle_keys(const kinetra::Input &input, kinetra::Camera2D &camera);
    void handle_pointer(const kinetra::Input &input, const kinetra::Camera2D &camera);
    void pick(Vec2 point, bool toggle);
    void show_for_a_while(std::string message);
    MenuEntry motion_switch(std::string_view label, bool MotionSettings::*setting);
    void add_view_entries(std::vector<MenuEntry> &entries);
    void add_motion_entries(std::vector<MenuEntry> &entries);

    std::uint32_t seed_;
    bool start_running_;
    unsigned updates_per_second_;
    std::optional<Simulation> simulation_;
    /// Setup of the loaded blueprint; R replays it.
    std::optional<Scenario> scenario_;
    ViewLayers layers_;
    std::optional<std::size_t> selected_mover_, selected_producer_;
    std::string message_;
    /// Seconds until the message expires; zero for a message that stays.
    double message_remaining_{};
    /// The menu shows the motion page instead of the view page.
    bool motion_page_{};
    bool fit_pending_{};
    /// Set by the BLUEPRINTS entry; the next begin_frame() closes the blueprint, so the
    /// frame that clicked the entry still finishes with its simulation.
    bool close_requested_{};
    lab::ModalPause modal_pause_;
};
} // namespace i4sim
