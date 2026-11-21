//
// Created by vpack on 2024-04-04.
//

#pragma once

#include "dotio/entities/moving_object.h"

namespace dotio {
struct Bolt : MovingObject {
    static constexpr float Radius = 5.f;
    explicit Bolt(dotio::math::Position position, dotio::math::Vector2 velocity);

    Bolt();

    void Update(float delta) override;

    void Draw(dotio::Renderer &renderer) override;
};
} // namespace dotio
