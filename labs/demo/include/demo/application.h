#pragma once

#include "demo/input/commands.h"
#include "demo/world/scene.h"
#include "demo/world/tuning.h"
#include "demo/zoom.h"
#include <kinetra/runtime/timing.h>

namespace demo {

/// Coordinates a sandbox session without a window or renderer. The caller supplies
/// elapsed time, intent and the current logical camera viewport, including on paused frames.
class Application {
  public:
    Application(int agent_count, std::uint32_t seed);
    /// Applies one frame of intent, then runs the fixed ticks that `elapsed` seconds times
    /// `commands.time_scale` are worth; none while paused, except one for
    /// `commands.single_step`. Throws std::invalid_argument, before anything changes, for
    /// a negative or non-finite time scale.
    void advance(double elapsed, const FrameCommands &commands, kinetra::Camera2D &camera);
    const Scene &scene() const { return scene_; }
    /// The scene's motion and wander values, for a debug menu to change between frames.
    /// The next tick throws std::invalid_argument for values that validate() rejects.
    Tuning &tuning() { return scene_.tuning(); }
    /// Read-only access to the scene's motion and wander values.
    const Tuning &tuning() const { return scene_.tuning(); }
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
    Scene scene_;
    kinetra::FixedClock clock_;
    PresentationOptions presentation_;
    bool paused_{}, gravity_{};
    ActionResult last_action_{};
    /// Every world body's pose before and after the last tick; presentation only.
    std::vector<Pose> before_tick_, after_tick_;
    /// Whether the previous frame was paused; the first running frame drops the old snapshots.
    bool was_paused_{};

    void apply_actions(const FrameCommands &commands);
    /// Runs one fixed tick. `snapshot` records every body's pose before and after it, for
    /// presented_pose().
    void tick(const FrameCommands &commands, bool snapshot);
    void update_camera(double elapsed, const FrameCommands &commands, kinetra::Camera2D &camera);
    void apply_pointer(const PointerCommand &command, const kinetra::Camera2D &camera);
};

} // namespace demo
