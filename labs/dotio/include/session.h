#pragma once
#include "../../settings.h"
#include "difficulty.h"
#include <cstdint>

namespace dotio {
std::unique_ptr<lab::Session> create_session(std::uint32_t seed, logic::Difficulty difficulty);
}
