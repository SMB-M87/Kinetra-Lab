#pragma once
// Where the busiest stretch of the captured i4sim floor begins, measured headless, so the
// i4sim captures show its movers driving whatever the simulation's timing is.
#include "capture/scenes/scenes.h"

#include <filesystem>
#include <span>
#include <vector>

namespace i4sim {
class FactoryController;
}

namespace capture {
/// Speed above which a mover counts as driving, in millimeters per tick; movers drive at 2.
inline constexpr float driving_speed = 0.3f;

/// Runs the actions of `clicks` on the debug menu of `controller`, each on the menu as the
/// click finds it, which is what the session does for a click on that row.
///
/// Throws std::runtime_error that names the click when its row does not exist or has another
/// label: the menu changed, and the clicks in scenes.h must follow. `controller` must have
/// a blueprint loaded.
void apply_menu_clicks(i4sim::FactoryController &controller, std::span<const MenuClick> clicks);

/// The number of movers driving after each of the first `steps` steps of Stage::step_seconds
/// of the i4sim floor `blueprint`, counted from the step after the one that starts it with
/// Space.
///
/// Runs the floor in a FactoryController, the headless half of the i4sim session, with the
/// stage's seed, update rate and step length and with i4sim_motion_clicks applied first, so
/// it is the run the captured session makes. Throws std::runtime_error when the blueprint
/// cannot be loaded or a click does not find its row, std::invalid_argument for a negative
/// `steps`.
std::vector<int> driving_movers(const std::filesystem::path &blueprint, int steps);

/// Index at which the `window` consecutive entries of `activity` with the largest sum begin,
/// among the indices that are multiples of `stride`; the earliest when several are equal.
/// Throws std::invalid_argument when `window` or `stride` is not positive or `activity` has
/// fewer than `window` entries.
int busiest_window_start(std::span<const int> activity, int window, int stride);
} // namespace capture
