#pragma once

#include "demo/application.h"
#include "demo/debug_panel.h"
#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/inspector_view.h>
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
    /// The statistics line at the bottom, with `fps` frames per second and, when the UPS row
    /// is not at real time, the `ups` measured and the `target_ups` it asks for.
    bool statistics{};
    double fps{}, ups{};
    std::uint64_t target_ups{real_time_ups};
    /// The settings panel or its toolbar.
    const kinetra::SettingsOverlay *settings{};
    /// The frame-time graph.
    const kinetra::FrameTimes *frame_times{};
    /// Inspector and its current snapshot, or null when there is no selected agent.
    const kinetra::InspectorView *inspector{};
    const kinetra::InspectorSnapshot *inspection{};
};

/// Present the latest scene without changing application state: the bodies between their
/// last two ticks, the debug `layers` that are on, and the `overlays`. Vectors, paths and
/// probes follow the application's own presentation options, which commands switch. Draws
/// one complete frame, from Runtime::begin() to Runtime::end(); the runtime's camera is
/// unchanged afterwards.
void render(kinetra::Runtime &runtime, const Application &application,
            const DebugLayers &layers = DebugLayers::none(), const FrameOverlays &overlays = {});
/// State summary for DemoSession::status(): agents, pause, chunks, camera, paths,
/// avoidance and the last pointer action.
std::string status_text(const Application &application);

} // namespace demo
