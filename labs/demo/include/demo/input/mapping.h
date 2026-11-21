#pragma once

#include "demo/input/commands.h"
#include <kinetra/view/input.h>

namespace demo {

/// Adapt runtime key transitions and held state to application-owned commands.
FrameCommands read_commands(const kinetra::Input &input);

} // namespace demo
