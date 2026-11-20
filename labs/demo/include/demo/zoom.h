#pragma once

#include <kinetra/view/camera.h>

/// Camera zoom policy of the Demo, in pixels per world unit. Kept apart from the scene
/// headers so the launcher can start a Demo without compiling the scene.
namespace demo {
/// Zoom range outside Debug Mode.
inline constexpr kinetra::ZoomLimits zoom_limits{17, 100};
/// Debug Mode lets the camera zoom further out, to inspect streaming and crowds.
inline constexpr kinetra::ZoomLimits debug_zoom_limits{5, 100};
/// Zoom of a new session.
inline constexpr float initial_zoom = zoom_limits.minimum;
} // namespace demo
