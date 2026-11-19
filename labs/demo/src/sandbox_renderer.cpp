#include "../include/sandbox_renderer.h"
#include "../include/help_overlay.h"
#include <lab/camera.h>
#include <string_view>

namespace sandbox {
using namespace kinetra;
namespace {

void draw_route(Runtime &runtime, Vec2 start, std::span<const Vec2> route, Color color) {
    for (const auto waypoint : route) {
        runtime.line(start, waypoint, 3.f / runtime.camera.pixels_per_unit, color);
        start = waypoint;
    }
    if (!route.empty())
        runtime.circle(route.back(), 0.2f, color);
}

void draw_body(Runtime &runtime, const Body &body, bool is_player, const PresentationOptions &options) {
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
        if (options.vectors)
            draw_debug_vector(runtime, body.position, body.velocity * 0.3f, layer_color::vectors);
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

void render(Runtime &runtime, const SandboxApplication &application, const DebugLayers &layers,
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
    const lab::ScopedCameraShift follow_drawn_player(
        runtime.camera,
        scene.following ? position(scene.player) - scene.world.body(scene.player).position : Vec2{});
    runtime.begin();
    // Distant Ghost terrain is sampled for presentation only; it owns no physics or agents.
    if (!scene.following) {
        const auto center = scene.world.body(scene.player).position;
        const auto half = runtime.camera.viewport * (1.25f / 10.f);
        const Aabb resident{center - half, center + half};
        if (scene.view.min.x < resident.min.x || scene.view.min.y < resident.min.y ||
            scene.view.max.x > resident.max.x || scene.view.max.y > resident.max.y) {
            for (const auto &[coordinate, bodies] : scene.preview.chunks())
                for (const auto &body : bodies)
                    if (scene.view.overlaps(bounds(body)) && !resident.overlaps(bounds(body)))
                        draw_body(runtime, body, false, options);
        }
    }
    std::size_t id = 0;
    for (const auto &simulated : scene.world.bodies()) {
        if (!scene.view.overlaps(bounds(simulated))) {
            ++id;
            continue;
        }
        const Body body = presented(id, simulated);
        draw_body(runtime, body, id == scene.player, options);
        if (layers.collisions) {
            if (body.shape == Shape::circle)
                draw_debug_circle(runtime, body.position, body.radius, layer_color::collisions);
            else if (body.shape == Shape::polygon) {
                for (std::size_t i = 0; i < body.vertices.size(); ++i)
                    runtime.line(body.position + rotated(body.vertices[i], body.angle),
                                 body.position +
                                     rotated(body.vertices[(i + 1) % body.vertices.size()], body.angle),
                                 1.f / runtime.camera.pixels_per_unit, layer_color::collisions);
            } else
                draw_debug_box(runtime, body.position, body.half_extents, body.angle,
                               layer_color::collisions);
        }
        ++id;
    }
    // Draw probes after every body so later terrain cannot overwrite the debug strokes.
    if (options.probes)
        for (std::size_t probe = 0; probe < scene.world.bodies().size(); ++probe) {
            const auto &simulated = scene.world.body(probe);
            if (simulated.motion == Motion::fixed || !scene.view.overlaps(bounds(simulated)))
                continue;
            const Body body = presented(probe, simulated);
            const auto direction =
                length(body.velocity) > 0.01f ? normalized(body.velocity) : rotated(Vec2{1, 0}, body.angle);
            for (float angle : {0.f, 2.35619449f, -2.35619449f})
                runtime.line(body.position,
                             body.position + rotated(direction, angle) * (angle == 0 ? 4.f : 2.f),
                             3.f / runtime.camera.pixels_per_unit, layer_color::probes);
        }
    if (options.paths) {
        draw_route(runtime, position(scene.player), scene.player_path, {0.6f, 1, 0.3f});
        for (const auto &agent : scene.agents())
            if (agent.active())
                draw_route(runtime, position(agent.body_id()), agent.route(), layer_color::paths);
    }
    if (layers.home_range) {
        // Every awake agent is tied to its home; the range itself would clutter the view for
        // a whole crowd, so only the selected agent shows it.
        const float width = 1.f / runtime.camera.pixels_per_unit;
        for (const auto &agent : scene.agents()) {
            if (!agent.active())
                continue;
            runtime.line(position(agent.body_id()), agent.home(), width, layer_color::home_range);
            runtime.circle(agent.home(), 3 * width, layer_color::home_range);
            if (scene.selected_agent && length(agent.home() - *scene.selected_agent) < 0.001f)
                draw_debug_circle(runtime, agent.home(), application.tuning().wander.range,
                                  layer_color::home_range);
        }
    }
    if (scene.selected_agent)
        for (const auto &agent : scene.agents())
            if (length(agent.home() - *scene.selected_agent) < 0.001f)
                draw_debug_circle(runtime, position(agent.body_id()), 0.8f, {1, 1, 0});
    if (layers.active_agents)
        for (const auto &agent : scene.agents())
            if (agent.active())
                runtime.circle(position(agent.body_id()), 0.18f, layer_color::active_agents);

    if (overlays.statistics) {
        const auto active = std::count_if(scene.agents().begin(), scene.agents().end(),
                                          [](const auto &agent) { return agent.active(); });
        auto text = "FPS " + std::to_string(int(overlays.fps)) + " AGENTS " + std::to_string(active) + "/" +
                    std::to_string(scene.agents().size()) + " CHUNKS " +
                    std::to_string(scene.terrain.chunks().size()) +
                    (scene.reciprocal_avoidance ? " AVOID LIMIT " + std::to_string(scene.avoidance_limited)
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
    if (overlays.frame_times)
        draw_frame_times(runtime, *overlays.frame_times);
    if (overlays.settings)
        overlays.settings->draw(runtime);
    runtime.end();
}

std::string status_text(const SandboxApplication &application) {
    const auto &scene = application.scene();
    return std::to_string(scene.agents().size()) + " agents | " +
           (application.paused() ? "PAUSED" : "running") + " | " +
           std::to_string(scene.terrain.chunks().size()) + " chunks | " +
           (scene.following ? "camera FOLLOW" : "camera FREE") + " | paths " +
           (application.presentation().paths ? "ON" : "OFF") +
           (scene.reciprocal_avoidance ? " | avoidance RECIPROCAL | " : " | avoidance LEGACY | ") +
           std::string(action_text(application.last_action()));
}

} // namespace sandbox
