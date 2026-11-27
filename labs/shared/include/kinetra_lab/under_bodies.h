#pragma once

#include "kinetra_lab/route.h"

#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/engine.h>

#include <algorithm>
#include <array>
#include <cstddef>

/// The part of a body's own debug line or arrow that a Lab application draws over that body.
/// Needs the runtime, so only presentations include it.
///
/// Lines and arrows that start at a body (its vectors, i4sim's forces) are drawn under every
/// moving body, as the routes are (route.h), so none hides a neighbor. Right after a body, its
/// own line is drawn again up to the body's edge, so a line shorter than the body still shows
/// and it stays visible whose line it is; the bodies drawn later cover it again.
namespace kinetra_lab {

/// Draws the part of `line` (a line `width` world units wide from `line[0]` to `line[1]`) that
/// lies within `radius` of its start, edges included.
inline void draw_line_start(kinetra::Runtime &runtime, std::array<kinetra::Vec2, 2> line, float width,
                            float radius, kinetra::Color color) {
    const std::array<kinetra::Vec2, 1> end{line[1]};
    const auto points = route_start(line[0], end, radius - width / 2);
    for (std::size_t i = 1; i < points.size(); ++i)
        runtime.line(points[i - 1], points[i], width, color);
}

/// Draws the part of the arrow that kinetra::draw_debug_vector draws for `position` and
/// `vector` that lies within `radius` of `position`: the whole arrow when it ends inside,
/// otherwise its shaft up to the edge.
inline void draw_vector_start(kinetra::Runtime &runtime, kinetra::Vec2 position, kinetra::Vec2 vector,
                              float radius, kinetra::Color color) {
    const float magnitude = kinetra::length(vector);
    if (magnitude == 0)
        return;
    // The arrow's length as draw_debug_vector draws it.
    const float size = std::max({magnitude, 1.5f, 24.f / runtime.camera.pixels_per_unit});
    const float width = 2.f / runtime.camera.pixels_per_unit;
    if (size <= radius - width / 2) {
        kinetra::draw_debug_vector(runtime, position, vector, color);
        return;
    }
    draw_line_start(runtime, {position, position + vector / magnitude * size}, width, radius, color);
}

} // namespace kinetra_lab
