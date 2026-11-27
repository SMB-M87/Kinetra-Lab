#pragma once
// The Demo's debug menu without a window: its rows, what they have set, and the part of a
// frame's commands they decide. The session (src/session.cpp) adds the menu widget, the
// settings panel and the drawing.

#include "demo/application.h"

#include "kinetra_lab/debug.h"
#include <kinetra/view/debug_menu.h>

#include <vector>

namespace demo {
/// The colors of the Demo's debug layers: what the renderer draws them in and what their
/// menu rows show as swatches.
namespace layer_color {
inline constexpr kinetra::Color vectors{0.8f, 0.95f, 0.4f}, probes{1, 0.65f, 0.2f}, paths{0.3f, 0.7f, 1},
    collisions{1, 0.4f, 0.4f}, active_agents{1, 1, 0}, home_range{0.75f, 0.5f, 1};
} // namespace layer_color

/// Which debug layers the Demo draws. A session starts with VECTORS and PATHS on, so a
/// scenario opens on motion and routes without the clutter of the other layers.
struct DebugLayers {
    /// Velocity arrows of the movers.
    bool vectors{true};
    /// The three feelers ahead of and behind every mover.
    bool probes{};
    /// The planned routes of the player and of agents that follow one.
    bool paths{true};
    /// Outlines of the collision shapes.
    bool collisions{};
    /// A dot on every agent that is awake.
    bool active_agents{};
    /// A line from every awake agent to its home, and the home range of the selected agent.
    bool home_range{};
    /// No layer: what the Demo draws outside Debug Mode.
    static constexpr DebugLayers none() { return {false, false, false, false, false, false}; }
};

/// Limits of the tuning rows, as {minimum, maximum, step, whether the step is a factor}.
/// They keep every value valid for demo::validate() and for the steering code.
namespace tuning_step {
inline constexpr kinetra::DebugStep max_speed{0.5f, 16, 1.25f, true}, max_acceleration{1, 64, 1.25f, true},
    response_time{0.05f, 2, 1.25f, true}, lookahead{0, 3, 0.1f}, clearance{0, 1, 0.05f},
    separation_radius{0, 4, 0.2f}, separation_weight{0, 2, 0.05f}, horizon{0.25f, 4, 0.25f},
    passing_bias{0, 0.5f, 0.05f}, home_range{2, 64, 1.25f, true}, wander_reach{1, 8, 0.5f},
    wander_jitter{0, 12, 1};
} // namespace tuning_step

/// The Demo's debug menu and the state its rows change.
///
/// One frame is: entries() for the menu's update, apply() on the frame's commands,
/// Application::advance(), observe(), and entries() again for drawing. The rows
/// follow the contract of kinetra::DebugEntry: they refer to the panel and to the
/// application, which must both outlive them. Not copyable.
class DebugPanel {
  public:
    /// `application` is borrowed and must outlive the panel.
    explicit DebugPanel(Application &application)
        : application_(application), gravity_(application.scene().world().gravity.y != 0) {}
    DebugPanel(const DebugPanel &) = delete;
    DebugPanel &operator=(const DebugPanel &) = delete;

    /// The rows in display order: the headings SIMULATION, LAYERS, MOTION and AGENTS, each
    /// followed by its rows while it is open, then, below a rule, ZOOM, GHOST CAMERA, RESET
    /// PARAMETERS and SCENARIOS. At most one section is open; SIMULATION is at the start.
    ///
    /// MOTION lists the rows of the chosen avoidance model only: HORIZON and PASSING BIAS
    /// for RECIPROCAL, LOOKAHEAD, SEPARATION and SEP WEIGHT for LEGACY. AGENTS lists WANDER
    /// REACH and WANDER JITTER only while agents wander by steering, and ends with CLICK SETS
    /// GOAL and PASSING.
    std::vector<kinetra::DebugEntry> entries();

    /// Completes `commands`, which hold the frame's keyboard and pointer intent and its
    /// pause request, with what the menu decides: layers, ghost camera, gravity, avoidance
    /// model, time scale, a pause or single tick asked for by a row, and the zoom steps of
    /// `wheel` and of the ZOOM row.
    ///
    /// Outside `debug_mode` no layer shows, gravity and the ghost camera are off and the
    /// wheel does not zoom. While `pointer_captured` (the pointer is on a panel) the wheel
    /// does not zoom either and the frame's pointer command is dropped. With CLICK SETS GOAL
    /// on, a walk click in debug mode becomes a goal for every awake agent.
    void apply(FrameCommands &commands, bool debug_mode, bool pointer_captured, float wheel);

    /// Remembers the camera's zoom for the ZOOM row; call after the application advanced.
    void observe(const kinetra::Camera2D &camera) { zoom_.observe(camera); }

    /// The layers to draw: the chosen ones in `debug_mode`, none outside it.
    DebugLayers layers(bool debug_mode) const { return debug_mode ? layers_ : DebugLayers::none(); }
    /// Whether the reciprocal avoidance model is chosen (the default) rather than legacy
    /// steering.
    bool reciprocal_avoidance() const { return reciprocal_; }
    /// Whether PASSING is on: agents on commanded travel pass each other in agreed lanes.
    bool passing() const { return passing_; }
    /// Simulated seconds per elapsed second, as the SPEED row set it.
    float time_scale() const { return time_scale_; }
    /// Whether SCENARIOS was clicked. The session then asks its host for the launcher's page
    /// of Demo scenarios (kinetra::SessionRequest::configurations), which ends the session,
    /// so the flag is never reset.
    bool scenarios_requested() const { return scenarios_requested_; }

  private:
    void add_simulation_entries(std::vector<kinetra::DebugEntry> &entries);
    void add_layer_entries(std::vector<kinetra::DebugEntry> &entries);
    void add_motion_entries(std::vector<kinetra::DebugEntry> &entries);
    void add_agent_entries(std::vector<kinetra::DebugEntry> &entries);
    void reset_parameters();

    Application &application_;
    kinetra::DebugFolds folds_{"SIMULATION"};
    kinetra_lab::ZoomControl zoom_;
    DebugLayers layers_;
    bool ghost_{}, gravity_{}, click_sets_goal_{}, reciprocal_{true}, passing_{};
    float time_scale_{1};
    /// Set by the RUNNING and SINGLE TICK rows; the next apply() turns them into commands.
    bool pause_requested_{}, step_requested_{};
    bool scenarios_requested_{};
};
} // namespace demo
