//
// Created by Priory on 3/8/2024.
//

#include "dotio/entities/bolt.h"

namespace dotio {
Bolt::Bolt(dotio::math::Position start, dotio::math::Vector2 start_velocity) : MovingObject(start, 0, false) {
    velocity = start_velocity;
}

Bolt::Bolt() : Bolt({}, {}) {
}

void Bolt::update(float delta) {
    MovingObject::update(delta);
}

void Bolt::draw(dotio::Renderer &renderer) {
    renderer.draw_circle({position}, radius, {1.0f, 0.3f, 0.0f, 1.0f});
}
} // namespace dotio
