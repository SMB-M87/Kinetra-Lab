#pragma once
// What the i4sim view shows, as plain data: shared by the headless controller, which
// changes it, and the drawing code, which reads it.
#include <kinetra/camera.h>
#include <kinetra/math.h>

#include <bitset>
#include <cstddef>
#include <cstdint>

namespace i4sim {
/// Camera zoom range in pixels per millimeter: from a whole hall to a single mover.
inline constexpr kinetra::ZoomLimits zoom_limits{0.005f, 10};

/// Converts a point or vector between factory space and camera space. Blueprints and the
/// simulation use millimeters with Y down, from the floor's top-left corner; the camera's
/// world has Y up. The conversion mirrors Y, so it is its own inverse.
constexpr kinetra::Vec2 flip_y(kinetra::Vec2 point) {
    return {point.x, -point.y};
}

/// A debug layer of the world view. Every layer starts selected; layers are drawn only in
/// debug mode.
enum class Layer : std::uint8_t {
    /// Cell outlines of the route grid.
    grid,
    /// Free cells tinted by their heatmap weight.
    heatmap,
    /// Each mover's remaining route, and its retreat and passing targets.
    paths,
    /// Steering components, velocity and acceleration of each mover.
    forces,
    /// Walls, and the junction and corridor zones with their exit reservations.
    borders,
    /// The radius within which a mover predicts conflicts.
    detection,
    /// Outline of each mover's collision box.
    contact_shapes,
};
/// Number of Layer values.
inline constexpr std::size_t layer_count = 7;

/// Which debug layers are selected: a set of Layer values.
class ViewLayers {
  public:
    /// Every layer selected: what a session starts with.
    ViewLayers() { selected_.set(); }
    /// No layer selected: what the world view draws outside debug mode.
    static ViewLayers none() {
        ViewLayers layers;
        layers.selected_.reset();
        return layers;
    }
    /// Whether `layer` is selected.
    bool selected(Layer layer) const { return selected_.test(index(layer)); }
    /// Selects `layer` when it is not, and the other way around.
    void toggle(Layer layer) { selected_.flip(index(layer)); }
    /// Whether the world view draws `layer`.
    bool shows(Layer layer) const { return selected(layer); }

  private:
    static std::size_t index(Layer layer) { return static_cast<std::size_t>(layer); }
    std::bitset<layer_count> selected_;
};
} // namespace i4sim
