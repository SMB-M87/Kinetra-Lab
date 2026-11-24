//
// Created by vpack on 2024-04-04.
//

#pragma once

#include "dotio/entities/moving_object.h"

namespace dotio {
/// The player's bolt: flies at constant velocity until World::update_bolts stops it at a wall,
/// a crate or an enemy.
struct Bolt : MovingObject {
    /// Radius of the visible bolt, in legacy pixels; hits count from its edge.
    static constexpr float radius = 5.f;
    /// A bolt at `position` flying at `velocity` legacy pixels per update; inactive until its
    /// pool slot activates it.
    explicit Bolt(kinetra::Vec2 position, kinetra::Vec2 velocity);

    Bolt();

    void update(float delta) override;

    void draw(dotio::Renderer &renderer) override;
};
} // namespace dotio
