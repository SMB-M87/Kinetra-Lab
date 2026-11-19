#pragma once
#include "application.h"
#include <kinetra/bitmap_text.h>
#include <kinetra/settings_view.h>

namespace dotio {
/// Smooth, bounded wall gradient in absolute cell coordinates; stable across rebasing.
kinetra::Color wall_color(int absolute_row, int absolute_column);
Commands read_commands(const kinetra::Input &input, const kinetra::Camera2D &camera);
/// Draws the world, the HUD, the debug menu and, last, the settings overlay.
void render(kinetra::Runtime &runtime, Application &application, const kinetra::SettingsOverlay &settings);
/// State summary for DotioSession::status(): difficulty, pause, health, mana, kills and
/// the debug and ghost modes.
std::string status_text(Application &application);
} // namespace dotio
