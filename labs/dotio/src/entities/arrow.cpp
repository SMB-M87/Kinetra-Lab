//
// Created by Priory on 3/8/2024.
//

#include "dotio/entities/arrow.h"

namespace dotio {
Arrow::Arrow(dotio::math::Position position, dotio::math::Vector2 velocity)
    : MovingObject(position, 0, false) {
    Velocity = velocity;
}

Arrow::Arrow() : Arrow({}, {}) {
}

void Arrow::update(float delta) {
    MovingObject::update(delta);
}

void Arrow::draw(dotio::Renderer &renderer) {
    renderer.draw_segment({Position, Position - (Velocity).get_normalized() * 30.0f},
                          {1.0f, 0.8f, 0.8f, 1.0f});
}
} // namespace dotio
