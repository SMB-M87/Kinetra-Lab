#include "../include/sandbox_renderer.h"
#include "../include/help_overlay.h"
#include <string_view>

namespace sandbox {
using namespace kinetra;
namespace {

void draw_route(Runtime &runtime, Vec2 start, std::span<const Vec2> route, Color color) {
    for (const auto waypoint : route) {
        runtime.line(start, waypoint, 0.07f, color);
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
    else
        runtime.box(body.position, body.half_extents, body.angle, color);
    if (body.motion != Motion::fixed) {
        const float size = body.shape == Shape::circle ? body.radius : body.half_extents.x;
        runtime.line(body.position, body.position + rotated({size, 0}, body.angle), 0.05f,
                     {0.03f, 0.08f, 0.1f});
        if (options.probes) {
            const auto direction =
                length(body.velocity) > 0.01f ? normalized(body.velocity) : rotated(Vec2{1, 0}, body.angle);
            for (float angle : {0.f, 2.35619449f, -2.35619449f})
                runtime.line(body.position,
                             body.position + rotated(direction, angle) * (angle == 0 ? 4.f : 2.f),
                             2.f / runtime.camera.pixels_per_unit, {1, 0.65f, 0.2f});
        }
        if (options.vectors)
            draw_debug_vector(runtime, body.position, body.velocity * 0.3f, {0.8f, 0.95f, 0.4f});
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

void render(Runtime &runtime, const SandboxApplication &application, const DebugMenu *menu,
            std::span<const DebugOption> debug_options, bool collisions, bool agents, bool statistics,
            double fps) {
    const auto &scene = application.scene();
    const auto &options = application.presentation();
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
    for (const auto &body : scene.world.bodies()) {
        if (!scene.view.overlaps(bounds(body))) {
            ++id;
            continue;
        }
        draw_body(runtime, body, id == scene.player, options);
        if (collisions) {
            if (body.shape == Shape::circle)
                draw_debug_circle(runtime, body.position, body.radius, {1, 0.4f, 0.4f});
            else
                draw_debug_box(runtime, body.position, body.half_extents, body.angle, {1, 0.4f, 0.4f});
        }
        ++id;
    }
    if (options.paths) {
        draw_route(runtime, scene.world.body(scene.player).position, scene.player_path, {0.6f, 1, 0.3f});
        for (const auto &agent : scene.agents())
            if (agent.active())
                draw_route(runtime, scene.world.body(agent.body_id()).position, agent.route(),
                           {0.3f, 0.7f, 1});
    }
    if (scene.selected_agent)
        for (const auto &agent : scene.agents())
            if (length(agent.home() - *scene.selected_agent) < 0.001f)
                draw_debug_circle(runtime, scene.world.body(agent.body_id()).position, 0.8f, {1, 1, 0});
    if (agents)
        for (const auto &agent : scene.agents())
            if (agent.active())
                runtime.circle(scene.world.body(agent.body_id()).position, 0.18f, {1, 1, 0});
    draw_help(runtime, options.help);
    if (statistics) {
        const auto active = std::count_if(scene.agents().begin(), scene.agents().end(),
                                          [](const auto &agent) { return agent.active(); });
        const auto text = "FPS " + std::to_string(int(fps)) + " AGENTS " + std::to_string(active) + "/" +
                          std::to_string(scene.agents().size()) + " CHUNKS " +
                          std::to_string(scene.terrain.chunks().size());
        draw_text(runtime, text, {10, runtime.camera.viewport.y - 20}, 1.5f, {1, 1, 0});
    }
    if (menu)
        menu->draw(runtime, debug_options);
    runtime.end();
}

std::string window_title(const SandboxApplication &application) {
    const auto &scene = application.scene();
    return "Kinetra | Demo | " + std::to_string(scene.agents().size()) + " agents | " +
           (application.paused() ? "PAUSED" : "running") + " | " +
           std::to_string(scene.terrain.chunks().size()) + " chunks | " +
           (scene.following ? "camera FOLLOW" : "camera FREE") + " | paths " +
           (application.presentation().paths ? "ON" : "OFF") + " | " +
           std::string(action_text(application.last_action())) +
           " | WASD/arrows move, ghost camera and layers in debug menu, H help";
}

} // namespace sandbox
