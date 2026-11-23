#pragma once
#include "i4sim/blueprint/blueprint.h"
#include "kinetra_lab/session.h"
#include <memory>
#include <optional>

namespace i4sim {
/// Camera zoom before a blueprint is loaded and fitted, in pixels per millimeter.
inline constexpr float initial_zoom = 25;
/// Creates the graphical i4sim session: the blueprint screen for the catalog in `directory`,
/// and after a choice the factory with its HUD, debug menu and settings panel.
///
/// Without `initial_blueprint` it opens on the blueprint screen, where Escape or M ask for
/// the menu; with one it loads that file, and stays on the blueprint screen with the error
/// when loading fails. `seed` seeds every simulation of the session, `running` starts a
/// loaded blueprint unpaused, and `updates_per_second` is its initial tick rate.
/// Throws std::invalid_argument for an update rate outside 1..10000.
std::unique_ptr<kinetra_lab::Session>
create_session(std::uint32_t seed = 42, std::filesystem::path directory = default_blueprint_directory(),
               std::optional<std::filesystem::path> initial_blueprint = std::nullopt, bool running = false,
               unsigned updates_per_second = 200);
} // namespace i4sim
