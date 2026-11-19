#pragma once
// Drawing of the factory floor: everything of an i4sim frame that is placed in the world.
#include "simulation.h"
#include "view.h"

#include <kinetra/engine.h>

#include <cstddef>
#include <optional>

namespace i4sim {
/// Clear color of a frame that shows a factory.
inline constexpr kinetra::Color world_background{0.045f, 0.055f, 0.07f};

/// Draws the factory inside a frame that the caller began with Runtime::begin: the floor
/// and its cells, the producers, the movers between their last two ticks, the layers that
/// `layers` turns on, and a name label for every mover and producer.
///
/// `selected_mover` gets a marker; an index outside the fleet marks nothing. Pass
/// ViewLayers::none() to draw the plain factory. Reads the simulation only.
void draw_world(kinetra::Runtime &runtime, const Simulation &simulation, const ViewLayers &layers,
                std::optional<std::size_t> selected_mover);
} // namespace i4sim
