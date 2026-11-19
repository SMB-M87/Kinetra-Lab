#pragma once
#include <cstdint>
#include <lab/session.h>
#include <memory>

namespace sandbox {
inline constexpr int default_agent_count = 48;
/// Creates a Demo session with `count` agents and terrain from `seed`. The caller sets the
/// camera to sandbox::initial_zoom for a fresh start.
std::unique_ptr<lab::Session> create_session(int count, std::uint32_t seed);
} // namespace sandbox
