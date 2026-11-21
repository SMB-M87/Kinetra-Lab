#pragma once

#include <kinetra/simulation/math.h>

namespace demo {

/// Player intent, independent of keyboard bindings and windowing.
struct PlayerControl {
    /// Wanted direction of travel in world axes (Y up); it is normalized before use, so
    /// only its direction counts. Zero stands still.
    kinetra::Vec2 direction{};
    /// Rotate counterclockwise or clockwise; both cancel.
    bool turn_left{}, turn_right{};
};

} // namespace demo
