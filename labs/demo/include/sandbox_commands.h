#pragma once

#include "player_control.h"
#include <optional>

namespace sandbox {

enum class PointerAction { walk, teleport, target_agents };
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
    std::optional<PointerCommand> pointer;
};

struct PresentationOptions {
    bool vectors{}, paths{}, help{}, probes{};
};

enum class ActionResult { none, walking, no_route, teleported, teleport_blocked, agent_goals_set };

} // namespace sandbox
