#include "demo/presentation/renderer.h"
#include "kinetra_lab/camera.h"
#include "kinetra_lab/inspector.h"
#include "kinetra_lab/route.h"
#include "kinetra_lab/under_bodies.h"
#include <algorithm>
#include <limits>
#include <string_view>
#include <vector>

namespace demo {
using namespace kinetra;
namespace {

/// Seconds of motion a velocity vector shows.
constexpr float velocity_vector_scale = 0.3f;

/// The player's route; agents' routes have the PATHS layer color.
constexpr Color player_route_color{0.6f, 1, 0.3f};

void draw_polyline(Runtime &runtime, std::span<const Vec2> points, Color color) {
    for (std::size_t i = 1; i < points.size(); ++i)
        runtime.line(points[i - 1], points[i],
                     kinetra_lab::route_line_pixels / runtime.camera.pixels_per_unit, color);
}

void draw_route(Runtime &runtime, Vec2 start, std::span<const Vec2> route, Color color) {
    for (const auto waypoint : route) {
        runtime.line(start, waypoint, kinetra_lab::route_line_pixels / runtime.camera.pixels_per_unit, color);
        start = waypoint;
    }
    if (!route.empty())
        runtime.circle(route.back(), 0.2f, color);
}

/// Radius of the largest circle around the body's position that stays inside the body, so the
/// start of its route never reaches a neighbor.
float inner_radius(const Body &body) {
    if (body.shape == Shape::circle)
        return body.radius;
    if (body.shape == Shape::polygon) {
        // Distance to the nearest edge, assuming the position lies inside the polygon.
        float nearest = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < body.vertices.size(); ++i)
            nearest = std::min(nearest, distance_to_segment({}, body.vertices[i],
                                                            body.vertices[(i + 1) % body.vertices.size()]));
        return body.vertices.empty() ? 0.f : nearest;
    }
    return std::min(body.half_extents.x, body.half_extents.y);
}

void draw_collision_shape(Runtime &runtime, const Body &body) {
    if (body.shape == Shape::circle)
        draw_debug_circle(runtime, body.position, body.radius, layer_color::collisions);
    else if (body.shape == Shape::polygon) {
        for (std::size_t i = 0; i < body.vertices.size(); ++i)
            runtime.line(body.position + rotated(body.vertices[i], body.angle),
                         body.position + rotated(body.vertices[(i + 1) % body.vertices.size()], body.angle),
                         1.f / runtime.camera.pixels_per_unit, layer_color::collisions);
    } else
        draw_debug_box(runtime, body.position, body.half_extents, body.angle, layer_color::collisions);
}

void draw_body(Runtime &runtime, const Body &body, bool is_player) {
    Color color = body.motion == Motion::fixed        ? Color{0.3f, 0.36f, 0.45f}
                  : body.motion == Motion::controlled ? Color{0.18f, 0.75f, 0.82f}
                                                      : Color{0.95f, 0.53f, 0.2f};
    if (is_player)
        color = {0.55f, 1.f, 0.35f};
    if (body.shape == Shape::circle)
        runtime.circle(body.position, body.radius, color);
    else if (body.shape == Shape::polygon) {
        std::vector<Vec2> vertices;
        for (auto vertex : body.vertices)
            vertices.push_back(body.position + rotated(vertex, body.angle));
        runtime.polygon(vertices, color);
    } else
        runtime.box(body.position, body.half_extents, body.angle, color);
    if (body.motion != Motion::fixed) {
        const float size = local_extents(body).x;
        runtime.line(body.position, body.position + rotated({size, 0}, body.angle), 0.05f,
                     {0.03f, 0.08f, 0.1f});
    }
}

std::string_view action_text(ActionResult result) {
    switch (result) {
    case ActionResult::none:
        return "";
    case ActionResult::walking:
        return "walking";
    case ActionResult::no_route:
        return "no route";
    case ActionResult::teleported:
        return "teleported";
    case ActionResult::teleport_blocked:
        return "teleport blocked";
    case ActionResult::agent_goals_set:
        return "agent goals set";
    }
    return "";
}

} // namespace

void render(Runtime &runtime, const Application &application, const DebugLayers &layers,
            const FrameOverlays &overlays) {
    const auto &scene = application.scene();
    const auto &options = application.presentation();
    // A moving body as drawn: its interpolated pose; fixed terrain never moves.
    const auto presented = [&](std::size_t id, const Body &body) {
        if (body.motion == Motion::fixed)
            return body;
        Body shown = body;
        const auto pose = application.presented_pose(id);
        shown.position = pose.position;
        shown.angle = pose.angle;
        return shown;
    };
    const auto position = [&](std::size_t id) { return application.presented_pose(id).position; };
    // The camera follows the drawn player, not the simulated one, or the player would
    // jitter on screen. Only for this frame's drawing: the session keeps its camera.
    const bool streamed = scene.scenario() == Scenario::terrain;
    const kinetra_lab::ScopedCameraShift follow_drawn_player(
        runtime.camera, streamed && scene.following()
                            ? position(scene.player()) - scene.world().body(scene.player()).position
                            : Vec2{});
    runtime.begin();
    // Distant Ghost terrain is sampled for presentation only; it owns no physics or agents.
    if (streamed && !scene.following()) {
        const auto center = scene.world().body(scene.player()).position;
        const auto half = runtime.camera.viewport * (1.25f / 10.f);
        const Aabb resident{center - half, center + half};
        if (scene.view().min.x < resident.min.x || scene.view().min.y < resident.min.y ||
            scene.view().max.x > resident.max.x || scene.view().max.y > resident.max.y) {
            for (const auto &[coordinate, bodies] : scene.preview().chunks())
                for (const auto &body : bodies)
                    if (scene.view().overlaps(bounds(body)) && !resident.overlaps(bounds(body)))
                        draw_body(runtime, body, false);
        }
    }
    // Each route the PATHS layer shows, by the id of the body that follows it.
    std::vector<std::span<const Vec2>> routes;
    if (options.paths) {
        routes.resize(scene.world().bodies().size());
        routes[scene.player()] = scene.player_path();
        for (const auto &agent : scene.agents())
            if (agent.active())
                routes[agent.body_id()] = agent.route();
    }
    const auto route_color = [&](std::size_t body) {
        return body == scene.player() ? player_route_color : layer_color::paths;
    };
    const auto draw_bodies = [&](bool moving) {
        for (std::size_t id = 0; id < scene.world().bodies().size(); ++id) {
            const auto &simulated = scene.world().body(id);
            if ((simulated.motion != Motion::fixed) != moving || !scene.view().overlaps(bounds(simulated)))
                continue;
            const Body body = presented(id, simulated);
            draw_body(runtime, body, id == scene.player());
            if (layers.collisions)
                draw_collision_shape(runtime, body);
            // The start of the body's own velocity vector and route, over this body only: the
            // bodies drawn after it cover them again where they overlap.
            if (moving && options.vectors)
                kinetra_lab::draw_vector_start(runtime, body.position, body.velocity * velocity_vector_scale,
                                               inner_radius(body), layer_color::vectors);
            if (id < routes.size() && !routes[id].empty())
                draw_polyline(runtime,
                              kinetra_lab::route_start(body.position, routes[id], inner_radius(body)),
                              route_color(id));
        }
    };
    // Terrain first, then the velocity vectors, the steering probes and every route, then
    // whatever moves, so none of them hides a player or an agent while they still cross the
    // terrain they pass.
    draw_bodies(false);
    if (options.vectors)
        for (std::size_t id = 0; id < scene.world().bodies().size(); ++id) {
            const auto &simulated = scene.world().body(id);
            if (simulated.motion == Motion::fixed || !scene.view().overlaps(bounds(simulated)))
                continue;
            draw_debug_vector(runtime, position(id), simulated.velocity * velocity_vector_scale,
                              layer_color::vectors);
        }
    if (options.probes)
        for (std::size_t probe = 0; probe < scene.world().bodies().size(); ++probe) {
            const auto &simulated = scene.world().body(probe);
            if (simulated.motion == Motion::fixed || !scene.view().overlaps(bounds(simulated)))
                continue;
            const Body body = presented(probe, simulated);
            const auto direction =
                length(body.velocity) > 0.01f ? normalized(body.velocity) : rotated(Vec2{1, 0}, body.angle);
            for (float angle : {0.f, 2.35619449f, -2.35619449f})
                runtime.line(body.position,
                             body.position + rotated(direction, angle) * (angle == 0 ? 4.f : 2.f),
                             3.f / runtime.camera.pixels_per_unit, layer_color::probes);
        }
    for (std::size_t body = 0; body < routes.size(); ++body)
        if (!routes[body].empty())
            draw_route(runtime, position(body), routes[body], route_color(body));
    draw_bodies(true);
    if (layers.home_range) {
        // Every awake agent is tied to its home; the range itself would clutter the view for
        // a whole crowd, so only the selected agent shows it.
        const float width = 1.f / runtime.camera.pixels_per_unit;
        for (const auto &agent : scene.agents()) {
            if (!agent.active())
                continue;
            runtime.line(position(agent.body_id()), agent.home(), width, layer_color::home_range);
            runtime.circle(agent.home(), 3 * width, layer_color::home_range);
            if (scene.selected_agent() && length(agent.home() - *scene.selected_agent()) < 0.001f)
                draw_debug_circle(runtime, agent.home(), application.tuning().wander.range,
                                  layer_color::home_range);
        }
    }
    if (scene.selected_agent())
        for (const auto &agent : scene.agents())
            if (length(agent.home() - *scene.selected_agent()) < 0.001f)
                draw_debug_circle(runtime, position(agent.body_id()), 0.8f, {1, 1, 0});
    if (layers.active_agents)
        for (const auto &agent : scene.agents())
            if (agent.active())
                runtime.circle(position(agent.body_id()), 0.18f, layer_color::active_agents);

    if (overlays.statistics) {
        const auto active = std::count_if(scene.agents().begin(), scene.agents().end(),
                                          [](const auto &agent) { return agent.active(); });
        const auto layout = streamed ? "CHUNKS " + std::to_string(scene.terrain().chunks().size())
                                     : std::string(scenario_label(scene.scenario()));
        auto text =
            "FPS " + std::to_string(int(overlays.fps)) + " AGENTS " + std::to_string(active) + "/" +
            std::to_string(scene.agents().size()) + " " + layout +
            (scene.reciprocal_avoidance() ? " AVOID LIMIT " + std::to_string(scene.avoidance_limited())
                                          : " AVOID LEGACY");
        // What the debug menu's SIMULATION rows did to time is not visible in the scene itself.
        if (application.paused())
            text += " PAUSED";
        else if (overlays.time_scale != 1)
            text += " SPEED " + debug_number_text(overlays.time_scale) + "X";
        const float width = std::min(runtime.camera.viewport.x, 20.f + float(text.size()) * 9);
        runtime.box(runtime.camera.screen_to_world({width / 2, runtime.camera.viewport.y - 15}),
                    Vec2{width / 2, 13} / runtime.camera.pixels_per_unit, 0, {0.025f, 0.035f, 0.06f});
        draw_text(runtime, text, {10, runtime.camera.viewport.y - 20}, 1.5f, {1, 1, 0});
    }
    if (overlays.menu)
        overlays.menu->draw(runtime, overlays.entries);
    if (overlays.menu)
        kinetra_lab::draw_inspector(runtime, *overlays.menu, overlays.inspector, overlays.inspection,
                                    overlays.frame_times);
    else if (overlays.frame_times)
        draw_frame_times(runtime, *overlays.frame_times);
    if (overlays.settings)
        overlays.settings->draw(runtime);
    runtime.end();
}

std::string status_text(const Application &application) {
    const auto &scene = application.scene();
    const bool streamed = scene.scenario() == Scenario::terrain;
    const auto layout = streamed ? std::to_string(scene.terrain().chunks().size()) + " chunks"
                                 : std::string(scenario_label(scene.scenario()));
    const auto camera = scene.following() ? (streamed ? "camera FOLLOW" : "camera FIXED") : "camera FREE";
    return std::to_string(scene.agents().size()) + " agents | " +
           (application.paused() ? "PAUSED" : "running") + " | " + layout + " | " + camera + " | paths " +
           (application.presentation().paths ? "ON" : "OFF") +
           (scene.reciprocal_avoidance() ? " | avoidance RECIPROCAL | " : " | avoidance LEGACY | ") +
           std::string(action_text(application.last_action()));
}

} // namespace demo
