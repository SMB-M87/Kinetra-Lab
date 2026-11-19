#include "dotio/entities/static_object.h"
#include "dotio/math/portable.h"

namespace dotio {
StaticObject::~StaticObject() = default;
StaticObject::StaticObject(const dotio::math::Position position, const float direction, bool active)
    : IsActive(active), Position(position), Direction(direction) {
}

void StaticObject::Draw(dotio::Renderer &) {
}
} // namespace dotio
