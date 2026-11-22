#pragma once

#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/inspector_view.h>

#include <algorithm>

namespace kinetra_lab {
/// Space for the inspector, in logical window pixels. Leave room for the debug menu
/// at the left, for statistics at the bottom and for the optional graph below the panel.
inline kinetra::Aabb inspector_area(const kinetra::DebugMenu &menu, kinetra::Vec2 viewport,
                                    bool frame_times) {
    constexpr float margin = 8;
    constexpr float hud_space = 70;
    const float left =
        menu.visible ? menu.origin(viewport).x +
                           kinetra::DebugMenu::panel_width * kinetra::DebugMenu::scale(viewport) + margin
                     : margin;
    const float graph_space = frame_times ? kinetra::FrameTimeGraph::total_height + margin : 0;
    // Keep the center of the world reachable while inspecting at the usual camera center.
    const float bottom = std::min(viewport.y / 2 - margin, viewport.y - hud_space - graph_space);
    return {{left, margin}, {viewport.x - margin, bottom}};
}

/// Draws the inspector and places the optional graph below it. A null snapshot keeps
/// the graph in its usual corner. Call within an active frame.
inline void draw_inspector(kinetra::Runtime &runtime, const kinetra::DebugMenu &menu,
                           const kinetra::InspectorView *view, const kinetra::InspectorSnapshot *snapshot,
                           const kinetra::FrameTimes *times) {
    const auto viewport = runtime.camera.viewport;
    auto graph = kinetra::FrameTimeGraph::origin(viewport);
    if (view && snapshot) {
        const auto area = inspector_area(menu, viewport, times != nullptr);
        const auto bounds = view->bounds(*snapshot, area);
        if (bounds.min.x < bounds.max.x) {
            view->draw(runtime, *snapshot, area);
            graph.y = bounds.max.y + 12;
        }
    }
    if (times)
        kinetra::draw_frame_times(runtime, *times, graph);
}
} // namespace kinetra_lab
