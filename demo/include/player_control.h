#pragma once

#include <kinetra/math.h>

namespace sandbox {

/// Player intent, independent of keyboard bindings and windowing.
struct PlayerControl {
    kinetra::Vec2 direction{};
    bool turn_left{}, turn_right{};
};

} // namespace sandbox
