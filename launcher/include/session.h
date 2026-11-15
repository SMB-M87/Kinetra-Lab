#pragma once
#include "menu.h"
#include "../../labs/settings.h"
#include <cstdint>
#include <memory>

namespace launcher {
using Session = lab::Session;
std::unique_ptr<Session> start(Variant variant, int agent_count, std::uint32_t seed,
                               kinetra::Camera2D &camera,
                               logic::Difficulty difficulty = logic::Difficulty::medium);
int run(kinetra::Runtime &runtime, int count, std::uint32_t seed,
        logic::Difficulty difficulty, std::optional<Variant> initial, int frame_limit);
} // namespace launcher
