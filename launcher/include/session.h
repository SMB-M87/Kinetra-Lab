#pragma once
#include "menu.h"
#include <cstdint>
#include <memory>

namespace launcher {
using Session = kinetra::Session;
std::unique_ptr<Session> start(Variant variant, int agent_count, std::uint32_t seed,
                               kinetra::Camera2D &camera,
                               logic::Difficulty difficulty = logic::Difficulty::medium);
} // namespace launcher
