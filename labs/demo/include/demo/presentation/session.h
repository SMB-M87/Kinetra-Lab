#pragma once
#include "kinetra_lab/session.h"
#include <cstdint>
#include <memory>

namespace demo {
inline constexpr int default_agent_count = 48;
/// Creates a Demo session with `count` agents and terrain from `seed`. The caller sets the
/// camera to demo::initial_zoom for a fresh start.
std::unique_ptr<kinetra_lab::Session> create_session(int count, std::uint32_t seed);
} // namespace demo
