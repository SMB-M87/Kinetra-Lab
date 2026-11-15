#include "static_object.h"
#include "portable.h"

namespace logic {
StaticObject::~StaticObject() = default;
StaticObject::StaticObject(const gmath::Position position, const float direction, bool active)
    : IsActive(active), Position(position), Direction(direction) {
}

void StaticObject::Draw(gamef::Renderer &) {
}
} // namespace logic
