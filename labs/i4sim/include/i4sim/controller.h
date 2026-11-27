#pragma once
// The headless half of an i4sim session: it owns the loaded simulation and turns input
// into commands. It needs no window, so every rule of the session's behavior is testable
// from plain input values. The graphical session (src/session.cpp) adds the panels and
// the drawing.
#include "i4sim/blueprint/scenario.h"
#include "i4sim/simulation/simulation.h"
#include "i4sim/simulation/view.h"

#include "kinetra_lab/debug.h"
#include "kinetra_lab/input.h"
#include <kinetra/view/camera.h>
#include <kinetra/view/debug_menu.h>
#include <kinetra/view/input.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace i4sim {
/// Gives the color a debug layer is drawn in, for the swatch of its menu row; empty for a
/// layer without one color. The drawing code owns the colors (layer_color in world_view.h).
using LayerColors = std::function<std::optional<kinetra::Color>(Layer)>;

/// What the panels in front of the world did with a frame's input.
struct PanelState {
    /// The pointer is on a panel or its button, so clicks and the wheel are not for the world.
    bool pointer_captured{};
    /// Debug mode is on; only then does the wheel zoom.
    bool debug_mode{true};
};

/// The RUN button that a blueprint shows while it awaits its start
/// (FactoryController::awaiting_start()): a box in screen pixels, 200 by 40, centered across
/// `viewport` 30 pixels from its top, below the toolbar and above the fitted floor, and
/// narrower when the viewport leaves less than 16 pixels on each side. It stays off the
/// middle of the floor, where movers are selected and given goals before the start.
kinetra::Aabb start_button(Vec2 viewport);

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
/// The entries follow the contract of kinetra::DebugEntry: call their functions only while
/// the controller is alive, and ask for new entries after any of them ran.
class FactoryController {
  public:
    /// `seed` seeds every simulation the controller builds. `running` starts every loaded
    /// blueprint unpaused; without it only a blueprint with a scenario section (a showcase
    /// scene) starts so. `updates_per_second` is the tick rate of a loaded blueprint.
    /// Throws std::invalid_argument for a rate outside 1..10000.
    FactoryController(std::uint32_t seed, bool running, unsigned updates_per_second);
    FactoryController(const FactoryController &) = delete;
    FactoryController &operator=(const FactoryController &) = delete;

    /// Loads the blueprint at `path` with its scenario section, if it has one, and replaces
    /// the loaded blueprint. The new simulation runs when the controller was built running or
    /// the blueprint has a scenario, and otherwise is paused and awaits its start; the
    /// selection and the message are cleared, a camera fit is pending and
    /// a close that the BLUEPRINTS entry requested for the old blueprint is dropped.
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
    /// Whether the blueprint waits for its first run: a load() or an R restart left it paused
    /// and it has not run since. The session shows start_button() while it does; a left
    /// click on the button, Space, P and the RUNNING entry end the wait.
    bool awaiting_start() const { return awaiting_start_ && !simulation_->running(); }

    /// Which debug layers are selected.
    const ViewLayers &layers() const { return layers_; }
    /// The mover that is inspected, by index into Simulation::movers().
    std::optional<std::size_t> selected_mover() const { return selected_mover_; }
    /// Close the inspector and forget the mover targeted by the next Shift+click.
    void clear_selected_mover() { selected_mover_.reset(); }
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

    /// Starts a frame: closes the blueprint on request and ages any transient message by
    /// `elapsed` seconds. Returns whether a blueprint remains loaded for simulation input.
    bool begin_frame(double elapsed);
    /// Pauses the simulation while a modal panel is `open` and restores the run state it
    /// had when the panel closes (kinetra_lab::ModalPause). Call once per frame.
    void suspend(bool open);
    /// The rows of the debug menu, in display order: the headings SIMULATION, LAYERS, MOTION
    /// and TRAFFIC, each followed by its rows while it is open, then, below a rule, ZOOM,
    /// FRAME SCENE and BLUEPRINTS. At most one section is open; SIMULATION is open at the
    /// start. A click on a heading opens that section and closes the other, or closes it
    /// when it was open (kinetra::DebugFolds). PROFILE and PLANNER open a list of their
    /// values below them; choosing a value closes the list. A layer row gets the swatch
    /// `colors` gives for its layer; without `colors` no row has one.
    std::vector<kinetra::DebugEntry> menu(const LayerColors &colors = {});
    /// Applies a frame of input and then advances the simulation by `elapsed` seconds.
    ///
    /// Space or P pause and resume. R restarts the blueprint with the current tick rate and
    /// motion settings: a blueprint with a scenario replays its goals and runs, any other
    /// waits paused for its start. G toggles the workload, F fits the
    /// camera, and 1 to 6 toggle grid, heatmap, paths, forces, borders and detection. Q and E
    /// halve and double the update rate within 1..10000. The camera pans with WASD or the
    /// arrows, and zooms with the wheel as `panels` allows and by the steps the menu's ZOOM
    /// row collected. While the blueprint awaits its start, a click on start_button() is the
    /// button's: a left click runs the blueprint and neither click reaches the floor.
    /// Otherwise a click selects the mover or producer under it. On empty ground,
    /// a left click clears only the producer selection. Right-click also holds or blocks
    /// the target; with Shift held it selects as a left click does, holding and blocking
    /// nothing. Shift+left-click sends the selected mover there. Pointer commands are skipped while
    /// `panels.pointer_captured`.
    ///
    /// A scene with `ScenarioRepeat::restart` replays once its movers have stood still for the
    /// pause (RestartTimer) and keeps its run state. If either restart fails to replay a
    /// scenario, the blueprint closes and displays the cause. Throws std::invalid_argument for a
    /// negative or non-finite `elapsed`, before any input is applied.
    void handle_input(double elapsed, const kinetra::Input &input, kinetra::Camera2D &camera,
                      PanelState panels);
    /// Fits the camera to the floor when a fit is pending: after a load and after the
    /// FRAME SCENE entry.
    void update_camera(kinetra::Camera2D &camera);

  private:
    void close();
    /// Builds the blueprint anew; it runs when `running` and otherwise awaits its start.
    void restart(bool running);
    /// Restarts a restarting scene when its RestartTimer says so; see handle_input().
    void restart_when_due();
    void toggle_pause();
    void change_motion(const std::function<void(MotionSettings &)> &change);
    void change_update_rate(bool faster);
    void fit(kinetra::Camera2D &camera);
    void handle_keys(const kinetra::Input &input, kinetra::Camera2D &camera);
    void handle_pointer(const kinetra::Input &input, const kinetra::Camera2D &camera);
    void pick(Vec2 point, bool toggle);
    void show_for_a_while(std::string message);

    kinetra::DebugEntry motion_check(std::string label, bool MotionSettings::*setting);
    kinetra::DebugEntry motion_number(std::string label, std::function<std::string()> value,
                                      std::function<void(MotionSettings &)> lower,
                                      std::function<void(MotionSettings &)> raise);
    void add_simulation_entries(std::vector<kinetra::DebugEntry> &entries);
    void add_layer_entries(std::vector<kinetra::DebugEntry> &entries, const LayerColors &colors);
    void add_motion_entries(std::vector<kinetra::DebugEntry> &entries);
    void add_traffic_entries(std::vector<kinetra::DebugEntry> &entries);

    std::uint32_t seed_;
    bool start_running_;
    /// Set while a simulation that a load or a restart left paused has not run yet.
    bool awaiting_start_{};
    unsigned updates_per_second_;
    std::optional<Simulation> simulation_;
    /// Setup of the loaded blueprint; R replays it.
    std::optional<Scenario> scenario_;
    /// When a restarting scene starts again; replaced with every new simulation.
    RestartTimer restart_timer_;
    ViewLayers layers_;
    std::optional<std::size_t> selected_mover_, selected_producer_;
    std::string message_;
    /// Seconds until the message expires; zero for a message that stays.
    double message_remaining_{};
    /// Which section and which list of the debug menu are open.
    kinetra::DebugFolds folds_{"SIMULATION"};
    /// The menu's ZOOM row and the steps its buttons asked for.
    kinetra_lab::ZoomControl zoom_;
    bool fit_pending_{};
    /// Set by the BLUEPRINTS entry; the next begin_frame() closes the blueprint, so the
    /// frame that clicked the entry still finishes with its simulation.
    bool close_requested_{};
    kinetra_lab::ModalPause modal_pause_;
};
} // namespace i4sim
