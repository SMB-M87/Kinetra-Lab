#pragma once

#include <algorithm>
#include <array>
#include <kinetra/runtime/help_view.h>
#include <string_view>

namespace sandbox {

/// Draw a dependency-free bitmap legend in logical screen pixels, after the world.
/// Converting each rectangle through the camera keeps the HUD independent of zoom and pan.
inline void draw_help(kinetra::Runtime &runtime, bool expanded) {
    using namespace kinetra;
    constexpr std::array<std::string_view, 11> lines{"CONTROLS   CTRL: SETTINGS",
                                                     "WASD / ARROWS  MOVE PLAYER",
                                                     "Q / E OR SHIFT + LEFT / RIGHT  ROTATE",
                                                     "CLICK  WALK / SELECT AGENT / SET GOAL",
                                                     "RIGHT CLICK  CLEAR SELECTION",
                                                     "WHEEL  ZOOM",
                                                     "SETTINGS  PAUSE / RESUME",
                                                     "BACKQUOTE  DEBUG MENU",
                                                     "GHOST: CLICK  TELEPORT PLAYER",
                                                     "SETTINGS  LAUNCHER",
                                                     "SETTINGS  EXIT"};
    kinetra::draw_help(runtime, lines, expanded);
}

} // namespace sandbox
