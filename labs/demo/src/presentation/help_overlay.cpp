#include "demo/presentation/help_overlay.h"

#include <kinetra/runtime/help_view.h>

#include <array>
#include <string_view>

namespace demo {
void draw_help(kinetra::Runtime &runtime, bool expanded) {
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
} // namespace demo
