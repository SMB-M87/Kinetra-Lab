#pragma once

#include "demo/input/player_control.h"
#include <cstdint>
#include <optional>

namespace demo {
/// Simulated seconds per tick: the scene's physics step, the same at every UPS.
inline constexpr double tick_seconds = 1.0 / 120;
/// The UPS at which the scene runs in real time.
inline constexpr std::uint64_t real_time_ups = 120;
/// Most ticks a frame runs when nothing else limits it: the backlog cap of the Demo's clock
/// before its UPS could be set. The session replaces it with what its kinetra::TickBudget
/// allows.
inline constexpr std::uint64_t default_tick_limit = 16;

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
    bool toggle_pause{}, toggle_vectors{}, toggle_gravity{}, toggle_paths{};
    /// Debug Mode is on; the ghost camera is on (only in Debug Mode); steering probes show.
    bool debug{}, ghost{}, probes{};
    /// Agents use reciprocal avoidance rather than legacy steering.
    bool reciprocal_avoidance{true};
    /// Agents on commanded travel pass each other head-on in agreed side lanes.
    bool passing{};
    /// Ticks per elapsed second, at least one: real_time_ups is real time, half of it half
    /// speed. The camera is not slowed down.
    std::uint64_t ups{real_time_ups};
    /// The most ticks this frame may run; a longer backlog is dropped, so a UPS the processor
    /// cannot reach slows the scene down instead of the window.
    std::uint64_t tick_limit{default_tick_limit};
    /// While the scene is paused, run exactly one fixed tick in this frame. Ignored while
    /// the scene runs.
    bool single_step{};
    /// The frame's click, if any.
    std::optional<PointerCommand> pointer;
};

/// What the renderer draws besides the scene, as the commands switched it.
struct PresentationOptions {
    /// Velocity arrows, planned routes and steering probes.
    bool vectors{}, paths{}, probes{};
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
