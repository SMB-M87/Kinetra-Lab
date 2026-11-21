#pragma once

#include "demo/input/commands.h"
#include <kinetra/view/input.h>

namespace demo {

/// Adapt runtime key transitions and held state to application-owned commands: WASD or the
/// arrows move the player and the ghost camera, Q and E (or Shift with Left and Right)
/// rotate, the wheel zooms, a click walks and a right click clears the selection. The
/// debug menu and the settings panel fill in the rest of the commands.
FrameCommands read_commands(const kinetra::Input &input);

} // namespace demo
