#include "demo/application.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace demo {
namespace {
constexpr kinetra::Vec2 falling_gravity{0, -9.81f};

std::vector<Application::Pose> poses(const kinetra::World &world) {
    std::vector<Application::Pose> result;
    result.reserve(world.bodies().size());
    for (const auto &body : world.bodies())
        result.push_back({body.position, body.angle});
    return result;
}
} // namespace

Application::Application(int agent_count, std::uint32_t seed, Scenario scenario)
    : scene_(agent_count, seed, scenario), gravity_(scenario == Scenario::gravity) {
    if (gravity_)
        scene_.set_gravity(falling_gravity);
}

void Application::apply_actions(const FrameCommands &commands) {
    if (commands.toggle_pause)
        paused_ = !paused_;
    if (commands.toggle_vectors)
        presentation_.vectors = !presentation_.vectors;
    if (commands.toggle_gravity)
        gravity_ = !gravity_;
    if (commands.toggle_paths)
        presentation_.paths = !presentation_.paths;
    presentation_.probes = commands.probes;
    scene_.set_gravity(gravity_ ? falling_gravity : kinetra::Vec2{});
}

void Application::update_camera(double elapsed, const FrameCommands &commands, kinetra::Camera2D &camera) {
    kinetra::zoom(camera, commands.zoom_steps, commands.debug ? debug_zoom_limits : zoom_limits);
    if (!scene_.following())
        camera.position += commands.camera_pan * (float(std::min(elapsed, 0.1)) * 20);
    scene_.follow(camera);
    // Keep floating-point coordinates inside the terrain and spatial-grid limits.
    camera.position.x = std::clamp(camera.position.x, -100000.f, 100000.f);
    camera.position.y = std::clamp(camera.position.y, -100000.f, 100000.f);
}

void Application::apply_pointer(const PointerCommand &command, const kinetra::Camera2D &camera) {
    const auto destination = camera.screen_to_world(command.screen_position);
    switch (command.action) {
    case PointerAction::clear_selection:
        scene_.clear_selection();
        break;
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

void Application::tick(const FrameCommands &commands, bool snapshot) {
    if (snapshot)
        before_tick_ = poses(scene_.world());
    scene_.update(float(clock_.step()), commands.debug && commands.ghost ? PlayerControl{} : commands.player);
    if (snapshot)
        after_tick_ = poses(scene_.world());
}

void Application::advance(double elapsed, const FrameCommands &commands, kinetra::Camera2D &camera) {
    if (!std::isfinite(commands.time_scale) || commands.time_scale < 0)
        throw std::invalid_argument("Time scale must be finite and not negative");
    scene_.set_reciprocal_avoidance(commands.reciprocal_avoidance);
    scene_.set_passing(commands.passing);
    apply_actions(commands);
    scene_.set_following(!(commands.debug && commands.ghost));
    update_camera(elapsed, commands, camera);
    scene_.stream(camera);
    if (commands.pointer &&
        (commands.pointer->action != PointerAction::teleport || (commands.debug && commands.ghost))) {
        auto pointer = *commands.pointer;
        if (commands.debug && commands.ghost && pointer.action != PointerAction::clear_selection)
            pointer.action = PointerAction::teleport;
        apply_pointer(pointer, camera);
        scene_.follow(camera);
        scene_.stream(camera);
    }
    // Consume clock time even when paused, so resuming cannot replay paused time. Only
    // the simulation runs on scaled time; the camera above keeps real time.
    const int steps = clock_.advance(elapsed * double(commands.time_scale));
    // Snapshots from before a pause would draw the first running frames behind the state
    // shown while paused; start from the current state instead.
    if (!paused_ && was_paused_)
        before_tick_ = after_tick_ = poses(scene_.world());
    was_paused_ = paused_;
    if (!paused_)
        // Only the last tick is drawn between, so only it needs snapshots.
        for (int i = 0; i < steps; ++i)
            tick(commands, i == steps - 1);
    else if (commands.single_step)
        tick(commands, true);
    scene_.follow(camera);
}

Application::Pose Application::presented_pose(std::size_t id) const {
    const auto &body = scene_.world().body(id);
    const Pose exact{body.position, body.angle};
    if (paused_ || id >= after_tick_.size() || id >= before_tick_.size())
        return exact;
    // Anything but the last tick moved this body, or the id now names another body.
    const auto &after = after_tick_[id];
    if (after.position != body.position || after.angle != body.angle)
        return exact;
    const auto blend = float(clock_.interpolation());
    const auto &before = before_tick_[id];
    return {kinetra::lerp(before.position, body.position, blend),
            kinetra::lerp_angle(before.angle, body.angle, blend)};
}

} // namespace demo
