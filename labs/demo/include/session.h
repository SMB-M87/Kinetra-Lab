#pragma once
#include <cstdint>
#include "../../settings.h"

namespace sandbox {
std::unique_ptr<lab::Session> create_session(int count, std::uint32_t seed);
}
