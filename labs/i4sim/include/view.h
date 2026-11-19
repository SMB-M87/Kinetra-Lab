#pragma once
// What the i4sim view shows, as plain data: shared by the headless controller, which
// changes it, and the drawing code, which reads it.
#include <kinetra/camera.h>
#include <kinetra/math.h>

namespace i4sim {
/// Camera zoom range in pixels per millimeter: from a whole hall to a single mover.
inline constexpr kinetra::ZoomLimits zoom_limits{0.005f, 10};

/// Converts a point or vector between factory space and camera space. Blueprints and the
/// simulation use millimeters with Y down, from the floor's top-left corner; the camera's
/// world has Y up. The conversion mirrors Y, so it is its own inverse.
constexpr kinetra::Vec2 flip_y(kinetra::Vec2 point) {
    return {point.x, -point.y};
}

/// The debug layers of the world view. All start on; they are drawn only in debug mode.
struct ViewLayers {
    /// Cell outlines of the route grid.
    bool grid{true};
    /// Free cells tinted by their heatmap weight.
    bool heatmap{true};
    /// Each mover's remaining route, and its retreat and passing targets.
    bool paths{true};
    /// Steering components, velocity and acceleration of each mover.
    bool forces{true};
    /// Walls, and the junction and corridor zones with their exit reservations.
    bool borders{true};
    /// The radius within which a mover predicts conflicts.
    bool detection{true};
    /// Outline of each mover's collision box.
    bool contact_shapes{true};
};
} // namespace i4sim
