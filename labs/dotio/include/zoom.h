#pragma once

#include <kinetra/camera.h>

/// Camera zoom policy of Dotio, in pixels per world unit. Kept apart from the world
/// headers so the launcher can start Dotio without compiling the world.
namespace dotio {
/// Zoom range outside Debug Mode.
inline constexpr kinetra::ZoomLimits zoom_limits{25, 50};
/// Debug Mode lets the camera zoom further out, to watch enemies stream in.
inline constexpr kinetra::ZoomLimits debug_zoom_limits{2.5f, 50};
/// Zoom of a new session: 25 pixels per world unit, the legacy coordinate scale.
inline constexpr float initial_zoom = zoom_limits.minimum;
} // namespace dotio
