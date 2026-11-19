#pragma once
#include "blueprint.h"
#include <lab/session.h>
#include <memory>
#include <optional>

namespace i4sim {
/// Camera zoom before a blueprint is loaded and fitted, in pixels per millimeter.
inline constexpr float initial_zoom = 25;
/// Creates an i4sim session. Without `initial_blueprint` it opens on the blueprint screen,
/// where Escape or M ask for the menu. `running` starts the simulation unpaused.
std::unique_ptr<lab::Session>
create_session(std::uint32_t seed = 42, std::filesystem::path directory = default_blueprint_directory(),
               std::optional<std::filesystem::path> initial_blueprint = std::nullopt, bool running = false,
               unsigned updates_per_second = 200, std::optional<std::filesystem::path> output = std::nullopt);
} // namespace i4sim
