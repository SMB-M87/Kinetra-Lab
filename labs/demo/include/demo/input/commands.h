#pragma once

#include "demo/input/player_control.h"
#include <optional>

namespace demo {

enum class PointerAction { walk, teleport, target_agents, clear_selection };
struct PointerCommand {
    PointerAction action{PointerAction::walk};
    kinetra::Vec2 screen_position{};
};

/// One frame of intent. Toggles occur once per frame; held controls feed every fixed tick.
/// Pointer positions are logical pixels, resolved after camera changes in the same frame.
struct FrameCommands {
    PlayerControl player;
    kinetra::Vec2 camera_pan{};
    float zoom_steps{};
    bool toggle_pause{}, toggle_vectors{}, toggle_gravity{}, toggle_paths{}, toggle_help{};
    bool debug{}, ghost{}, probes{};
    bool reciprocal_avoidance{true};
    /// Simulated seconds per elapsed second: 1 is real time, 0.5 half speed, 0 stands
    /// still. It must be finite and not negative. The camera is not slowed down.
    float time_scale{1};
    /// While the scene is paused, run exactly one fixed tick in this frame. Ignored while
    /// the scene runs.
    bool single_step{};
    std::optional<PointerCommand> pointer;
};

struct PresentationOptions {
    bool vectors{}, paths{}, help{}, probes{};
};

enum class ActionResult { none, walking, no_route, teleported, teleport_blocked, agent_goals_set };

} // namespace demo
