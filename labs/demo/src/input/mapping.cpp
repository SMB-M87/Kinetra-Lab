#include "demo/input/mapping.h"

#include "kinetra_lab/input.h"

namespace demo {
FrameCommands read_commands(const kinetra::Input &input) {
    using kinetra::Key;
    FrameCommands commands;
    commands.player.direction = kinetra_lab::direction(input);
    commands.player.turn_left = input.down(Key::q) || (input.down(Key::shift) && input.down(Key::left));
    commands.player.turn_right = input.down(Key::e) || (input.down(Key::shift) && input.down(Key::right));
    // Shift turns the arrows into rotation, so only A and D still move sideways.
    if (input.down(Key::shift))
        commands.player.direction.x = float(input.down(Key::d)) - float(input.down(Key::a));
    commands.camera_pan = commands.player.direction;
    commands.zoom_steps = input.wheel;

    if (input.right_clicked)
        commands.pointer = PointerCommand{PointerAction::clear_selection, input.mouse};
    else if (input.clicked)
        commands.pointer = PointerCommand{PointerAction::walk, input.mouse};
    return commands;
}
} // namespace demo
