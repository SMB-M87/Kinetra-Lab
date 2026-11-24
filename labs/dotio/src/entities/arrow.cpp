//
// Created by Priory on 3/8/2024.
//

#include "dotio/entities/arrow.h"

namespace dotio {
Arrow::Arrow(dotio::math::Position start, dotio::math::Vector2 start_velocity)
    : MovingObject(start, 0, false) {
    velocity = start_velocity;
}

Arrow::Arrow() : Arrow({}, {}) {
}

void Arrow::update(float delta) {
    MovingObject::update(delta);
}

void Arrow::draw(dotio::Renderer &renderer) {
    renderer.draw_segment({position, position - kinetra::normalized(velocity) * 30.0f},
                          {1.0f, 0.8f, 0.8f, 1.0f});
}
} // namespace dotio
