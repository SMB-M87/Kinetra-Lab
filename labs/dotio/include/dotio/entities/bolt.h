//
// Created by vpack on 2024-04-04.
//

#pragma once

#include "dotio/entities/moving_object.h"

namespace dotio {
struct Bolt : MovingObject {
    static constexpr float radius = 5.f;
    explicit Bolt(dotio::math::Position position, dotio::math::Vector2 velocity);

    Bolt();

    void update(float delta) override;

    void draw(dotio::Renderer &renderer) override;
};
} // namespace dotio
