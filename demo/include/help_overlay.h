#pragma once

#include <algorithm>
#include <array>
#include <kinetra/help_view.h>
#include <string_view>

namespace sandbox {

/// Draw a dependency-free bitmap legend in logical screen pixels, after the world.
/// Converting each rectangle through the camera keeps the HUD independent of zoom and pan.
inline void draw_help(kinetra::Runtime &runtime, bool expanded) {
    using namespace kinetra;
    constexpr std::array<std::string_view, 10> lines{"CONTROLS   H: HIDE HELP",
                                                     "WASD / ARROWS  MOVE PLAYER",
                                                     "Q / E OR SHIFT + LEFT / RIGHT  ROTATE",
                                                     "CLICK  WALK / SELECT AGENT / SET GOAL",
                                                     "WHEEL  ZOOM",
                                                     "SPACE  PAUSE / RESUME",
                                                     "BACKQUOTE  DEBUG MENU",
                                                     "GHOST: CLICK  TELEPORT PLAYER",
                                                     "M  START MENU",
                                                     "ESC  EXIT"};
    kinetra::draw_help(runtime, lines, expanded);
}

} // namespace sandbox
