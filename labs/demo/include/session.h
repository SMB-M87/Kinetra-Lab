#pragma once
#include "../../settings.h"
#include <cstdint>

namespace sandbox {
inline constexpr int default_agent_count = 48;
std::unique_ptr<lab::Session> create_session(int count, std::uint32_t seed);
} // namespace sandbox
