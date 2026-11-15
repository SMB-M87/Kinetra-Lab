#pragma once

#include "sandbox_commands.h"
#include "sandbox_scene.h"
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

  private:
    Sandbox scene_;
    kinetra::FixedClock clock_;
    PresentationOptions presentation_;
    bool paused_{}, gravity_{};
    ActionResult last_action_{};

    void apply_actions(const FrameCommands &commands);
    void update_camera(double elapsed, const FrameCommands &commands, kinetra::Camera2D &camera);
    void apply_pointer(const PointerCommand &command, const kinetra::Camera2D &camera);
};

} // namespace sandbox
