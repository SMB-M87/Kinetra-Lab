#pragma once

#include "sandbox_commands.h"
#include <kinetra/engine.h>

namespace sandbox {

/// Adapt runtime key transitions and held state to application-owned commands.
inline FrameCommands read_commands(const kinetra::Input &input) {
    using kinetra::Key;
    FrameCommands commands;
    commands.player.direction = {float(input.down(Key::d) || input.down(Key::right)) -
                                     float(input.down(Key::a) || input.down(Key::left)),
                                 float(input.down(Key::w) || input.down(Key::up)) -
                                     float(input.down(Key::s) || input.down(Key::down))};
    commands.player.turn_left = input.down(Key::q) || (input.down(Key::shift) && input.down(Key::left));
    commands.player.turn_right = input.down(Key::e) || (input.down(Key::shift) && input.down(Key::right));
    if (input.down(Key::shift))
        commands.player.direction.x = float(input.down(Key::d)) - float(input.down(Key::a));
    commands.camera_pan = commands.player.direction;
    commands.zoom_steps = input.wheel;
    commands.toggle_pause = input.hit(Key::space);

    commands.toggle_help = input.hit(Key::h);

    if (input.clicked) {
        const auto action = PointerAction::walk;
        commands.pointer = PointerCommand{action, input.mouse};
    }
    return commands;
}

} // namespace sandbox
