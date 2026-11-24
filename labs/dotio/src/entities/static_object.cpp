#include "dotio/entities/static_object.h"

namespace dotio {
StaticObject::~StaticObject() = default;
StaticObject::StaticObject(const kinetra::Vec2 start, const float start_direction, bool active)
    : is_active(active), position(start), direction(start_direction) {
}

void StaticObject::draw(dotio::Renderer &) {
}
} // namespace dotio
