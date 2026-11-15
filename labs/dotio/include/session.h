#pragma once
#include "difficulty.h"
#include <cstdint>
#include <kinetra/session.h>

namespace dotio {
std::unique_ptr<kinetra::Session> create_session(std::uint32_t seed, logic::Difficulty difficulty);
}
