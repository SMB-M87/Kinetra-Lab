#pragma once

#include "demo/debug_panel.h"
#include "demo/sandbox_application.h"
#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/settings_view.h>
#include <span>
#include <string>

namespace demo {

/// What a Demo frame draws in screen space, over the scene. Everything is borrowed for the
/// call; a null pointer or a false flag leaves that piece out.
struct FrameOverlays {
    /// The debug menu and the rows it draws; pass the rows the menu was updated with.
    const kinetra::DebugMenu *menu{};
    std::span<const kinetra::DebugEntry> entries{};
    /// The statistics line at the bottom, with `fps` frames per second and, when it is not
    /// 1, the `time_scale` the scene runs at.
    bool statistics{};
    double fps{};
    float time_scale{1};
    /// The settings panel or its toolbar.
    const kinetra::SettingsOverlay *settings{};
    /// The frame-time graph.
    const kinetra::FrameTimes *frame_times{};
};

/// Present the latest scene without changing application state: the bodies between their
/// last two ticks, the debug `layers` that are on, and the `overlays`. Vectors, paths and
/// probes follow the application's own presentation options, which commands switch.
void render(kinetra::Runtime &runtime, const SandboxApplication &application,
            const DebugLayers &layers = DebugLayers::none(), const FrameOverlays &overlays = {});
/// State summary for DemoSession::status(): agents, pause, chunks, camera, paths,
/// avoidance and the last pointer action.
std::string status_text(const SandboxApplication &application);

} // namespace demo
