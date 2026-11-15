//
// Created by vpack on 2024-04-04.
//

#ifndef MAIN_BOLT_H
#define MAIN_BOLT_H

#include "moving_object.h"

namespace logic {
struct Bolt : MovingObject {
    static constexpr float Radius = 5.f;
    explicit Bolt(gmath::Position position, gmath::Vector2 velocity);

    Bolt();

    void Update(float delta) override;

    void Draw(gamef::Renderer &renderer) override;
};
} // namespace logic

#endif // MAIN_BOLT_H
