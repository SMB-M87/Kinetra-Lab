#pragma once

#include <kinetra/simulation/math.h>

namespace demo {

/// Player intent, independent of keyboard bindings and windowing.
struct PlayerControl {
    kinetra::Vec2 direction{};
    bool turn_left{}, turn_right{};
};

} // namespace demo
