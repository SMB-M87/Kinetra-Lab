#pragma once
#include <cstdint>
#include <kinetra/session.h>

namespace sandbox {
std::unique_ptr<kinetra::Session> create_session(int count, std::uint32_t seed);
}
