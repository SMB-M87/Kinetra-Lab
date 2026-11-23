#pragma once

#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/inspector_view.h>

#include <algorithm>

namespace kinetra_lab {
/// Left edge of the space beside the debug menu, in logical pixels.
inline float inspector_left(const kinetra::DebugMenu &menu, kinetra::Vec2 viewport) {
    constexpr float margin = 8;
    return menu.visible ? menu.origin(viewport).x +
                              kinetra::DebugMenu::panel_width * kinetra::DebugMenu::scale(viewport) + margin
                        : margin;
}

/// Scale that fits the graph and its left border in that space.
inline float inspector_graph_scale(const kinetra::DebugMenu &menu, kinetra::Vec2 viewport) {
    const float available = viewport.x - 12 - inspector_left(menu, viewport);
    return std::clamp(available / (kinetra::FrameTimeGraph::width + 4), 0.f, 1.f);
}

/// Space for the inspector, in logical window pixels. Leave room for the debug menu
/// at the left, for statistics at the bottom and for the optional graph below the panel.
inline kinetra::Aabb inspector_area(const kinetra::DebugMenu &menu, kinetra::Vec2 viewport,
                                    bool frame_times) {
    constexpr float margin = 8;
    constexpr float hud_space = 70;
    const float graph_space =
        frame_times ? kinetra::FrameTimeGraph::total_height * inspector_graph_scale(menu, viewport) + margin
                    : 0;
    // Keep the center of the world reachable while inspecting at the usual camera center.
    const float bottom = std::min(viewport.y / 2 - margin, viewport.y - hud_space - graph_space);
    return {{inspector_left(menu, viewport), margin}, {viewport.x - margin, bottom}};
}

/// Draws the inspector and places the optional graph below it. A null snapshot keeps
/// the graph in its usual corner. Call within an active frame.
inline void draw_inspector(kinetra::Runtime &runtime, const kinetra::DebugMenu &menu,
                           const kinetra::InspectorView *view, const kinetra::InspectorSnapshot *snapshot,
                           const kinetra::FrameTimes *times) {
    const auto viewport = runtime.camera.viewport;
    const float scale = inspector_graph_scale(menu, viewport);
    auto graph = kinetra::FrameTimeGraph::origin(viewport);
    graph.x += kinetra::FrameTimeGraph::width * (1 - scale);
    if (view && snapshot) {
        const auto area = inspector_area(menu, viewport, times != nullptr);
        const auto bounds = view->bounds(*snapshot, area);
        if (bounds.min.x < bounds.max.x) {
            view->draw(runtime, *snapshot, area);
            graph.y = bounds.max.y + 12;
        }
    }
    if (times && scale > 0)
        kinetra::draw_frame_times(runtime, *times, graph, scale);
}
} // namespace kinetra_lab
