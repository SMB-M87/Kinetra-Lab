#include "dotio/entities/static_object.h"

namespace dotio {
StaticObject::~StaticObject() = default;
StaticObject::StaticObject(const dotio::math::Position position, const float direction, bool active)
    : IsActive(active), Position(position), Direction(direction) {
}

void StaticObject::draw(dotio::Renderer &) {
}
} // namespace dotio
