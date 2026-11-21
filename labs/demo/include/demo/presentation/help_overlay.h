#pragma once

#include <kinetra/runtime/engine.h>

namespace demo {

/// Draw a dependency-free bitmap legend in logical screen pixels, after the world.
/// Converting each rectangle through the camera keeps the HUD independent of zoom and pan.
void draw_help(kinetra::Runtime &runtime, bool expanded);

} // namespace demo
