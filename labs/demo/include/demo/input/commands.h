#pragma once

#include "demo/input/player_control.h"
#include <optional>

namespace demo {

/// What a click asks for.
enum class PointerAction {
    /// Select the agent under the pointer, command the selected agent there, or
    /// else walk the player there.
    walk,
    /// Move the player there at once; only with the ghost camera.
    teleport,
    /// Command every awake agent there.
    target_agents,
    /// Forget the selected agent.
    clear_selection,
};
/// One click of a frame.
struct PointerCommand {
    PointerAction action{PointerAction::walk};
    /// Where the click was, in logical window pixels.
    kinetra::Vec2 screen_position{};
};

/// One frame of intent. Toggles occur once per frame; held controls feed every fixed tick.
/// Pointer positions are logical pixels, resolved after camera changes in the same frame.
struct FrameCommands {
    /// Held movement and rotation, applied on every tick of the frame.
    PlayerControl player;
    /// Direction the ghost camera moves in, in world axes; its length scales the speed.
    kinetra::Vec2 camera_pan{};
    /// Mouse-wheel notches, positive to zoom in.
    float zoom_steps{};
    /// Each flips its setting once in this frame.
    bool toggle_pause{}, toggle_vectors{}, toggle_gravity{}, toggle_paths{}, toggle_help{};
    /// Debug Mode is on; the ghost camera is on (only in Debug Mode); steering probes show.
    bool debug{}, ghost{}, probes{};
    /// Agents use reciprocal avoidance rather than legacy steering.
    bool reciprocal_avoidance{true};
    /// Simulated seconds per elapsed second: 1 is real time, 0.5 half speed, 0 stands
    /// still. It must be finite and not negative. The camera is not slowed down.
    float time_scale{1};
    /// While the scene is paused, run exactly one fixed tick in this frame. Ignored while
    /// the scene runs.
    bool single_step{};
    /// The frame's click, if any.
    std::optional<PointerCommand> pointer;
};

/// What the renderer draws besides the scene, as the commands switched it.
struct PresentationOptions {
    /// Velocity arrows, planned routes, the help overlay and steering probes.
    bool vectors{}, paths{}, help{}, probes{};
};

/// Outcome of the last pointer action, for the status text.
enum class ActionResult {
    /// No pointer action yet.
    none,
    /// The player walks along a planned route.
    walking,
    /// No route reaches the clicked point.
    no_route,
    teleported,
    /// The teleport destination overlaps a body or lies outside the view.
    teleport_blocked,
    /// Every awake agent got the clicked point as its goal.
    agent_goals_set,
};

} // namespace demo
