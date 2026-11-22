#pragma once

#include "i4sim/simulation/simulation.h"

#include <kinetra/view/inspector.h>

namespace i4sim {
/// Snapshot of the mover at `index`, using simulation units. Throws std::out_of_range
/// if the index is outside the simulation's mover list.
kinetra::InspectorSnapshot inspect_mover(const Simulation &simulation, std::size_t index);

} // namespace i4sim
