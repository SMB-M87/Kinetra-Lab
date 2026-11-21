//
// Created by Priory on 3/8/2024.
//

#pragma once

#include "dotio/entities/moving_object.h"

namespace dotio {
/// An enemy's arrow: flies at constant velocity until World::update_arrows stops it at a wall,
/// a crate or the player.
struct Arrow : MovingObject {
    /// An arrow at `position` flying at `velocity` legacy pixels per update; inactive until
    /// its pool slot activates it.
    explicit Arrow(dotio::math::Position position, dotio::math::Vector2 velocity);

    Arrow();

    void update(float delta) override;

    /// Draws a 30-pixel trail behind the tip.
    void draw(dotio::Renderer &renderer) override;
};
} // namespace dotio
