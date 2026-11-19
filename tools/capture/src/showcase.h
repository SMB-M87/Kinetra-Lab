#pragma once
// The length of one cycle of an i4sim showcase scene, measured headless, so a showcase clip
// ends where the scene starts over and its GIF loops without a jump.
#include <filesystem>

namespace capture {
/// Steps of Stage::step_seconds that one cycle of the showcase scene `blueprint` lasts, from
/// the step after the one that starts it with Space.
///
/// Runs the scene in a FactoryController, the headless half of the i4sim session, with the
/// stage's seed, update rate and step length, so it counts the steps the captured session
/// takes. A cycle ends at the step in which a mover sets off again after every mover has
/// stood still: the second time for a shuttle (out, pause, back, pause), the first time for
/// a restarting scene (run, pause, restart). The returned step is the first of the next
/// cycle. Throws std::invalid_argument when `blueprint` has no repeating scenario,
/// std::runtime_error when it cannot be loaded or does not complete a cycle within
/// two minutes.
int showcase_cycle_steps(const std::filesystem::path &blueprint);
} // namespace capture
