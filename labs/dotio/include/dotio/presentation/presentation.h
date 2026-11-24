#pragma once
#include "dotio/application.h"
#include <kinetra/runtime/bitmap_text.h>
#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/settings_view.h>
#include <utility>
#include <vector>

namespace kinetra {
class InspectorView;
}

namespace dotio {
/// Smooth, bounded wall gradient in absolute cell coordinates; stable across rebasing.
kinetra::Color wall_color(int absolute_row, int absolute_column);
Commands read_commands(const kinetra::Input &input, const kinetra::Camera2D &camera);
/// Moves the player, enemies, arrows and bolts to their interpolated positions for one
/// frame's drawing, and puts the simulated positions back when destroyed. The ported logic
/// draws itself from `Position`, so the positions are swapped in rather than passed down.
/// An object whose current position differs from its snapshot after the last tick (a
/// teleport, a respawn into the slot) keeps its exact position. `application` is borrowed
/// and must outlive the object.
class PresentedPositions {
  public:
    explicit PresentedPositions(Application &application);
    ~PresentedPositions();
    PresentedPositions(const PresentedPositions &) = delete;
    PresentedPositions &operator=(const PresentedPositions &) = delete;

  private:
    std::vector<std::pair<dotio::StaticObject *, kinetra::Vec2>> simulated_;
};
/// Draws the world, the HUD, `menu` with the application's debug rows while it is visible,
/// and, last, the settings overlay. Moving objects are drawn between their last two ticks
/// (PresentedPositions).
void render(kinetra::Runtime &runtime, Application &application, const kinetra::SettingsOverlay &settings,
            const kinetra::DebugMenu &menu, const kinetra::InspectorView *inspector = nullptr);
/// State summary for DotioSession::status(): difficulty, pause, health, mana, kills and
/// the debug and ghost modes.
std::string status_text(Application &application);
} // namespace dotio
