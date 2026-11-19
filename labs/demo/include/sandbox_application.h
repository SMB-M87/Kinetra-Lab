#pragma once

#include "sandbox_commands.h"
#include "sandbox_scene.h"
#include "zoom.h"
#include <kinetra/timing.h>

namespace sandbox {

/// Coordinates a sandbox session without a window or renderer. The caller supplies
/// elapsed time, intent and the current logical camera viewport, including on paused frames.
class SandboxApplication {
  public:
    SandboxApplication(int agent_count, std::uint32_t seed);
    void advance(double elapsed, const FrameCommands &commands, kinetra::Camera2D &camera);
    const Sandbox &scene() const { return scene_; }
    const PresentationOptions &presentation() const { return presentation_; }
    bool paused() const { return paused_; }
    ActionResult last_action() const { return last_action_; }
    /// Position and angle of a body as drawn.
    struct Pose {
        kinetra::Vec2 position;
        float angle{};
    };
    /// Pose of world body `id` for drawing: between its states before and after the last
    /// fixed tick, by how much of the next tick has accumulated, so motion stays even when
    /// frames run different numbers of ticks. A body that changed since that tick (teleport,
    /// streaming that renumbered bodies) or a paused scene shows its exact state. Throws
    /// std::out_of_range for an invalid id.
    Pose presented_pose(std::size_t id) const;

  private:
    Sandbox scene_;
    kinetra::FixedClock clock_;
    PresentationOptions presentation_;
    bool paused_{}, gravity_{};
    ActionResult last_action_{};
    /// Every world body's pose before and after the last tick; presentation only.
    std::vector<Pose> before_tick_, after_tick_;

    void apply_actions(const FrameCommands &commands);
    void update_camera(double elapsed, const FrameCommands &commands, kinetra::Camera2D &camera);
    void apply_pointer(const PointerCommand &command, const kinetra::Camera2D &camera);
};

} // namespace sandbox
