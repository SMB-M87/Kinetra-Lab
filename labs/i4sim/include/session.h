#pragma once
#include "../../settings.h"
#include "blueprint.h"
#include <optional>

namespace i4sim {
std::unique_ptr<lab::Session>
create_session(std::uint32_t seed = 42, std::filesystem::path directory = default_blueprint_directory(),
               std::optional<std::filesystem::path> initial_blueprint = std::nullopt, bool running = false,
               unsigned updates_per_second = 200, std::optional<std::filesystem::path> output = std::nullopt);
} // namespace i4sim
