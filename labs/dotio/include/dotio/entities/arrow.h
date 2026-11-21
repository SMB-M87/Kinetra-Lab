//
// Created by Priory on 3/8/2024.
//

#pragma once

#include "dotio/entities/moving_object.h"

namespace dotio {
struct Arrow : MovingObject {
    explicit Arrow(dotio::math::Position position, dotio::math::Vector2 velocity);

    Arrow();

    void Update(float delta) override;

    void Draw(dotio::Renderer &renderer) override;
};
} // namespace dotio
