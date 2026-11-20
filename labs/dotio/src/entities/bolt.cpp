#include "dotio/math/portable.h"
//
// Created by Priory on 3/8/2024.
//

#include "dotio/entities/bolt.h"

namespace dotio {
Bolt::Bolt(dotio::math::Position position, dotio::math::Vector2 velocity) : MovingObject(position, 0, false) {
    Velocity = velocity;
}

Bolt::Bolt() : Bolt({}, {}) {
}

void Bolt::Update(float delta) {
    MovingObject::Update(delta);
}

void Bolt::Draw(dotio::Renderer &renderer) {
    renderer.DrawCircle({Position}, Radius, {1.0f, 0.3f, 0.0f, 1.0f});
}
} // namespace dotio
