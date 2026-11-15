#pragma once
#include "difficulty.h"
#include <cstdint>
#include "../../settings.h"

namespace dotio {
std::unique_ptr<lab::Session> create_session(std::uint32_t seed, logic::Difficulty difficulty);
}
