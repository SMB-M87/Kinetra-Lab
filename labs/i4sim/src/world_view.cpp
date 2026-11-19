#include "world_view.h"

#include <kinetra/bitmap_text.h>
#include <kinetra/debug_view.h>

#include <algorithm>
#include <string_view>
#include <vector>

namespace i4sim {
namespace {
using namespace kinetra;

constexpr Color floor_color{0.13f, 0.15f, 0.18f};
constexpr Color blocked_cell_color{0.065f, 0.075f, 0.09f};
constexpr Color grid_line_color{0.3f, 0.35f, 0.4f};
/// Heatmap weight that tints a cell fully; a large mover adds 16 per corner.
constexpr float full_heat_weight = 16;
constexpr Color mover_color{.7f, .7f, .7f};
/// Held movers and movers in contact.
constexpr Color mover_alert_color{1, 0, 0};
constexpr Color selection_color{1, 1, 0};
constexpr Color blocked_producer_color{0.8f, 0.15f, 0.2f};
constexpr Color route_color{0.7f, 0.4f, 1};
constexpr Color retreat_target_color{1, 0.3f, 0.7f};
constexpr Color passing_target_color{0.3f, 1, 0.7f};
constexpr Color junction_color{0, 0.8f, 1};
constexpr Color corridor_color{0.2f, 0.5f, 1};
constexpr Color exit_reservation_color{0.6f, 1, 0};
constexpr Color border_color{1, 0.4f, 0.1f};
/// Arrow length per unit, so per-tick values are visible at factory scale: steering
/// components and acceleration are in millimeters per tick squared, velocity in millimeters
/// per tick.
constexpr float steering_arrow_scale = 50, velocity_line_scale = 40, acceleration_line_scale = 100;

/// Color of a producer by model: the robots have their brand colors, the manual station
/// is blue-gray.
Color producer_color(Model model) {
    switch (model) {
    case Model::Kuka:
        return {.75f, .375f, 0};
    case Model::Staubli:
        return {.8f, .7f, .1f};
    case Model::Viper:
        return {1, 1, 1};
    default:
        return {.35f, .5f, .65f};
    }
}

/// World length of `pixels` screen pixels, so line widths and markers keep their size on
/// screen at every zoom.
float screen_pixels(const Runtime &runtime, float pixels) {
    return pixels / runtime.camera.pixels_per_unit;
}

/// Half the larger cell side: the radius of a producer's symbol.
float producer_radius(const Blueprint &blueprint) {
    return std::max(blueprint.cell_size.x, blueprint.cell_size.y) / 2;
}

void draw_floor(Runtime &runtime, const Simulation &simulation, const ViewLayers &layers) {
    const auto &blueprint = simulation.blueprint();
    const auto &grid = simulation.grid();
    const float line_width = screen_pixels(runtime, 1);
    runtime.box(flip_y(blueprint.dimension / 2), blueprint.dimension / 2, 0, floor_color);
    for (int id = 0; id < grid.width() * grid.height(); ++id) {
        const auto center = flip_y(grid.center(id));
        if (grid.blocked(id))
            runtime.box(center, blueprint.cell_size / 2, 0, blocked_cell_color);
        else if (layers.heatmap) {
            const float weight = std::min(1.f, grid.weight(id) / full_heat_weight);
            runtime.box(center, blueprint.cell_size / 2, 0, {0.13f + weight * 0.6f, 0.15f, 0.2f});
        }
        if (layers.grid) {
            // The top and left edge of every cell make the grid; the floor's far edges
            // stay open.
            const auto corner = grid.center(id) - blueprint.cell_size / 2,
                       opposite = corner + blueprint.cell_size;
            runtime.line(flip_y(corner), flip_y({opposite.x, corner.y}), line_width, grid_line_color);
            runtime.line(flip_y(corner), flip_y({corner.x, opposite.y}), line_width, grid_line_color);
        }
    }
}

void draw_producers(Runtime &runtime, const Simulation &simulation) {
    const auto &blueprint = simulation.blueprint();
    const float radius = producer_radius(blueprint);
    const Vec2 extent{blueprint.mover_max_extent, blueprint.mover_max_extent};
    for (const auto &producer : simulation.producers()) {
        const auto &definition = producer.definition;
        const Color color = producer.blocked ? blocked_producer_color : producer_color(definition.model);
        if (definition.rectangular)
            runtime.box(flip_y(definition.position), {radius, radius}, 0, color);
        else
            runtime.circle(flip_y(definition.position), radius, color);
        // The processing place, darker, with a dot while a product is being processed.
        const auto processing = flip_y(definition.processing_position + extent / 2);
        runtime.box(processing, extent / 2, 0, {color.r * 0.4f, color.g * 0.4f, color.b * 0.4f});
        if (producer.product)
            runtime.circle(processing, radius / 4, color);
    }
}

/// The debug layers of one mover, drawn at `position` (factory space).
void draw_mover_layers(Runtime &runtime, const Simulation &simulation, std::size_t index, Vec2 position,
                       const ViewLayers &layers) {
    const auto &mover = simulation.movers()[index];
    const auto &motion = simulation.motion_settings();
    const auto center = flip_y(position);
    if (layers.contact_shapes)
        draw_debug_box(runtime, center, mover.body.half_extents, 0, mover_alert_color);
    if (layers.detection)
        draw_debug_circle(runtime, center, length(mover.body.half_extents) + motion.speed * motion.horizon,
                          {0.25f, 0.4f, 0.6f});
    if (layers.paths) {
        Vec2 from = position;
        for (auto target : mover.route) {
            runtime.line(flip_y(from), flip_y(target), screen_pixels(runtime, 2), route_color);
            from = target;
        }
        if (const auto target = simulation.corridor_retreat_target(index))
            draw_debug_vector(runtime, center, flip_y(*target - position), retreat_target_color);
        if (const auto target = simulation.passing_target(index))
            draw_debug_vector(runtime, center, flip_y(*target - position), passing_target_color);
    }
    if (layers.forces) {
        const auto &forces = mover.forces;
        draw_debug_vector(runtime, center, flip_y(forces.border * steering_arrow_scale), {1, .5f, 0});
        draw_debug_vector(runtime, center, flip_y(forces.collision * steering_arrow_scale), {1, 0, 0});
        draw_debug_vector(runtime, center, flip_y(forces.predictive * steering_arrow_scale), {1, 0, 1});
        draw_debug_vector(runtime, center, flip_y(forces.seek * steering_arrow_scale), {0, 1, 0});
        runtime.line(center, flip_y(position + mover.body.velocity * velocity_line_scale),
                     screen_pixels(runtime, 3), {0, 1, 1});
        runtime.line(center, flip_y(position + forces.acceleration * acceleration_line_scale),
                     screen_pixels(runtime, 2), {1, 1, 0});
    }
}

/// Where a mover is drawn: between its last two ticks, so motion stays even when a frame
/// runs three ticks and the next one four.
Vec2 shown_position(const Mover &mover, float blend) {
    return lerp(mover.previous_position, mover.body.position, blend);
}

void draw_movers(Runtime &runtime, const Simulation &simulation, const ViewLayers &layers,
                 std::optional<std::size_t> selected, float blend) {
    for (std::size_t i = 0; i < simulation.movers().size(); ++i) {
        const auto &mover = simulation.movers()[i];
        const Vec2 position = shown_position(mover, blend);
        runtime.box(flip_y(position), mover.body.half_extents, 0,
                    (mover.held || mover.contacting) ? mover_alert_color : mover_color);
        if (selected == i)
            runtime.circle(flip_y(position), screen_pixels(runtime, 6), selection_color);
        draw_mover_layers(runtime, simulation, i, position, layers);
    }
}

void draw_zone(Runtime &runtime, const Aabb &area, Color color) {
    draw_debug_box(runtime, flip_y((area.min + area.max) / 2), (area.max - area.min) / 2, 0, color);
}

/// Junctions and corridors under traffic control, each with the exit space it reserved.
void draw_traffic_zones(Runtime &runtime, const Simulation &simulation) {
    const auto &motion = simulation.motion_settings();
    if (motion.junction_traffic)
        for (std::size_t i = 0; i < simulation.junction_count(); ++i) {
            draw_debug_box(runtime, flip_y(simulation.junction_center(i)),
                           simulation.blueprint().cell_size / 2, 0, junction_color);
            if (const auto exit = simulation.junction_exit_reservation(i))
                draw_zone(runtime, *exit, exit_reservation_color);
        }
    if (motion.corridor_traffic)
        for (std::size_t i = 0; i < simulation.corridor_count(); ++i) {
            draw_zone(runtime, simulation.corridor_bounds(i), corridor_color);
            if (const auto exit = simulation.corridor_exit_reservation(i))
                draw_zone(runtime, *exit, exit_reservation_color);
        }
}

/// Places name labels in screen space above their anchors and moves a label up until it
/// is clear of the earlier ones, so dense parking rows stay readable.
class LabelStack {
  public:
    explicit LabelStack(Runtime &runtime) : runtime_(runtime) {}

    /// Draws `name` above `position` (factory space), with a line to its anchor.
    void add(std::string_view name, Vec2 position) {
        const auto &camera = runtime_.camera;
        const Vec2 anchor = camera.world_to_screen(flip_y(position));
        const float width = float(name.size()) * 6;
        Vec2 label = anchor + Vec2{-width / 2, -16};
        Aabb box = bounds(label, width);
        for (std::size_t attempt = 0; attempt <= placed_.size() && overlaps_placed(box); ++attempt) {
            label.y -= 14;
            box = bounds(label, width);
        }
        placed_.push_back(box);
        runtime_.line(camera.screen_to_world(anchor), camera.screen_to_world(label + Vec2{width / 2, 7}),
                      screen_pixels(runtime_, 1), {.7f, .7f, .7f});
        runtime_.box(camera.screen_to_world(label + Vec2{width / 2, 4}),
                     Vec2{width / 2 + 3, 7} / camera.pixels_per_unit, 0, {.025f, .035f, .06f});
        draw_text(runtime_, name, label, 1, {1, 1, 1});
    }

  private:
    /// Screen box of a label's text plus a three-pixel margin.
    static Aabb bounds(Vec2 label, float width) { return {label - Vec2{3, 3}, label + Vec2{width + 3, 10}}; }
    bool overlaps_placed(const Aabb &box) const {
        return std::any_of(placed_.begin(), placed_.end(),
                           [&](const Aabb &other) { return box.overlaps(other); });
    }

    Runtime &runtime_;
    std::vector<Aabb> placed_;
};
} // namespace

void draw_world(Runtime &runtime, const Simulation &simulation, const ViewLayers &layers,
                std::optional<std::size_t> selected_mover) {
    const float blend = float(simulation.interpolation());
    draw_floor(runtime, simulation, layers);
    draw_producers(runtime, simulation);
    draw_movers(runtime, simulation, layers, selected_mover, blend);
    if (layers.borders) {
        draw_traffic_zones(runtime, simulation);
        for (const auto &segment : simulation.borders())
            runtime.line(flip_y(segment.start), flip_y(segment.end), screen_pixels(runtime, 2), border_color);
    }
    LabelStack labels(runtime);
    for (const auto &mover : simulation.movers())
        labels.add(mover.definition.id, shown_position(mover, blend));
    for (const auto &producer : simulation.producers())
        labels.add(producer.definition.id, producer.definition.position);
}
} // namespace i4sim
