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
/// debug mode. PATHS, FORCES and BORDERS consist of parts, which are layers of their own:
/// a part is drawn only while it and its layer are selected (parent_layer).
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
    /// Part of PATHS: the line along each mover's remaining route.
    routes,
    /// Part of PATHS: the arrow to a mover's corridor retreat target.
    retreat_targets,
    /// Part of PATHS: the arrow to a mover's predictive passing target.
    passing_targets,
    /// Part of FORCES: the steering component toward the next route point.
    seek_force,
    /// Part of FORCES: the steering component away from other movers.
    collision_force,
    /// Part of FORCES: the steering component away from walls.
    border_force,
    /// Part of FORCES: the steering component of predictive avoidance.
    predictive_force,
    /// Part of FORCES: the line along each mover's velocity.
    velocity,
    /// Part of FORCES: the line along each mover's acceleration.
    acceleration,
    /// Part of BORDERS: the wall segments.
    walls,
    /// Part of BORDERS: the junction and corridor zones with their exit reservations.
    traffic_zones,
};
/// Number of Layer values.
inline constexpr std::size_t layer_count = 18;

/// The layer that `layer` is a part of, or `layer` itself for a top-level layer.
constexpr Layer parent_layer(Layer layer) {
    switch (layer) {
    case Layer::routes:
    case Layer::retreat_targets:
    case Layer::passing_targets:
        return Layer::paths;
    case Layer::seek_force:
    case Layer::collision_force:
    case Layer::border_force:
    case Layer::predictive_force:
    case Layer::velocity:
    case Layer::acceleration:
        return Layer::forces;
    case Layer::walls:
    case Layer::traffic_zones:
        return Layer::borders;
    default:
        return layer;
    }
}

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
    /// Whether the world view draws `layer`: it is selected, and so is the layer it is a part
    /// of. Turning a layer off hides its parts but keeps their selection.
    bool shows(Layer layer) const { return selected(layer) && selected(parent_layer(layer)); }

  private:
    static std::size_t index(Layer layer) { return static_cast<std::size_t>(layer); }
    std::bitset<layer_count> selected_;
};
} // namespace i4sim
