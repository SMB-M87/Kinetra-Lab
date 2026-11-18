#pragma once
#include <cstdint>
#include "../../settings.h"

namespace sandbox {
inline constexpr int default_agent_count = 48;
std::unique_ptr<lab::Session> create_session(int count, std::uint32_t seed);
}
