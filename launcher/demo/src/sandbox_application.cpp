#include "../include/sandbox_application.h"
#include <algorithm>
#include <cmath>

namespace sandbox {

SandboxApplication::SandboxApplication(int agent_count, std::uint32_t seed) : scene_(agent_count, seed) {
}

void SandboxApplication::apply_actions(const FrameCommands &commands) {
    if (commands.toggle_pause)
        paused_ = !paused_;
    if (commands.toggle_vectors)
        presentation_.vectors = !presentation_.vectors;
    if (commands.toggle_gravity)
        gravity_ = !gravity_;
    if (commands.toggle_paths)
        presentation_.paths = !presentation_.paths;
    if (commands.toggle_help)
        presentation_.help = !presentation_.help;
    presentation_.probes = commands.probes;
    scene_.world.gravity = gravity_ ? kinetra::Vec2{0, -9.81f} : kinetra::Vec2{};
}

void SandboxApplication::update_camera(double elapsed, const FrameCommands &commands,
                                       kinetra::Camera2D &camera) {
    camera.pixels_per_unit = std::clamp(camera.pixels_per_unit * std::pow(1.1f, commands.zoom_steps),
                                        commands.debug ? 5.f : 17.f, 100.f);
    if (!scene_.following)
        camera.position += commands.camera_pan * (float(std::min(elapsed, 0.1)) * 20);
    scene_.follow(camera);
    // Keep floating-point coordinates inside the terrain and spatial-grid limits.
    camera.position.x = std::clamp(camera.position.x, -100000.f, 100000.f);
    camera.position.y = std::clamp(camera.position.y, -100000.f, 100000.f);
}

void SandboxApplication::apply_pointer(const PointerCommand &command, const kinetra::Camera2D &camera) {
    const auto destination = camera.screen_to_world(command.screen_position);
    switch (command.action) {
    case PointerAction::teleport:
        last_action_ =
            scene_.teleport(destination) ? ActionResult::teleported : ActionResult::teleport_blocked;
        break;
    case PointerAction::target_agents:
        scene_.target_agents(destination);
        last_action_ = ActionResult::agent_goals_set;
        break;
    case PointerAction::walk:
        if (scene_.select_or_target(destination))
            break;
        last_action_ = scene_.walk_to(destination) ? ActionResult::walking : ActionResult::no_route;
        break;
    }
}

void SandboxApplication::advance(double elapsed, const FrameCommands &commands, kinetra::Camera2D &camera) {
    apply_actions(commands);
    scene_.following = !(commands.debug && commands.ghost);
    update_camera(elapsed, commands, camera);
    scene_.stream(camera);
    if (commands.pointer &&
        (commands.pointer->action != PointerAction::teleport || (commands.debug && commands.ghost))) {
        auto pointer = *commands.pointer;
        if (commands.debug && commands.ghost)
            pointer.action = PointerAction::teleport;
        apply_pointer(pointer, camera);
        scene_.follow(camera);
        scene_.stream(camera);
    }
    // Consume clock time even when paused, so resuming cannot replay paused time.
    const int steps = clock_.advance(elapsed);
    if (!paused_)
        for (int i = 0; i < steps; ++i)
            scene_.update(float(clock_.step()),
                          commands.debug && commands.ghost ? PlayerControl{} : commands.player);
    scene_.follow(camera);
}

} // namespace sandbox
