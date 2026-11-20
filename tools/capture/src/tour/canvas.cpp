#include "capture/tour/canvas.h"

#include <kinetra/runtime/bitmap_text.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace capture {
namespace {
using kinetra::Color;
using kinetra::Vec2;

/// The arrow pointer at half its drawn size: `X` outline, `.` fill, space nothing.
constexpr std::array<std::string_view, 19> pointer_shape{
    "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ",
    "X....X      ", "X.....X     ", "X......X    ", "X.......X   ", "X........X  ",
    "X.........X ", "X......XXXXX", "X...X..X    ", "X..XX..X    ", "X.X  X..X   ",
    "XX   X..X   ", "X     X..X  ", "      X..X  ", "       XX   "};
/// Pixels per cell of pointer_shape.
constexpr int pointer_scale = 2;
static_assert(pointer_shape.size() * pointer_scale == pointer_height);
static_assert(pointer_shape[0].size() * pointer_scale == pointer_width);

/// Text size and colors of a keycap.
constexpr int keycap_text_scale = 2;
constexpr Color keycap_face{0.10f, 0.14f, 0.22f}, keycap_edge{0.45f, 0.62f, 0.90f}, keycap_text{1, 1, 1};

/// Blends `color` at `coverage` of its alpha over pixel (x, y); ignores pixels outside.
void blend(Canvas &canvas, int x, int y, Color color, float coverage = 1) {
    if (x < 0 || y < 0 || x >= canvas.width || y >= canvas.height)
        return;
    const float alpha = std::clamp(color.a * coverage, 0.f, 1.f);
    auto *pixel = &canvas.rgb[(static_cast<std::size_t>(y) * static_cast<std::size_t>(canvas.width) +
                               static_cast<std::size_t>(x)) *
                              3];
    const float source[3]{color.r, color.g, color.b};
    for (int channel = 0; channel < 3; ++channel) {
        const float mixed = static_cast<float>(pixel[channel]) * (1 - alpha) +
                            std::clamp(source[channel], 0.f, 1.f) * 255 * alpha;
        pixel[channel] = static_cast<std::uint8_t>(std::lround(mixed));
    }
}

Color faded(Color color, float strength) {
    color.a *= std::clamp(strength, 0.f, 1.f);
    return color;
}
} // namespace

Canvas::Canvas(int frame_width, int frame_height) : width(frame_width), height(frame_height) {
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("A canvas needs a positive size");
    rgb.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3, 0);
}

Color Canvas::pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height)
        throw std::out_of_range("Pixel outside the canvas");
    const auto *at =
        &rgb[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) *
             3];
    return {static_cast<float>(at[0]) / 255, static_cast<float>(at[1]) / 255,
            static_cast<float>(at[2]) / 255};
}

int text_width(std::string_view text, int scale) {
    return text.empty() ? 0 : (static_cast<int>(text.size()) * glyph_cell_width - 1) * scale;
}

void fill(Canvas &canvas, Vec2 corner, Vec2 size, Color color) {
    const int left = std::max(0, static_cast<int>(std::lround(corner.x)));
    const int top = std::max(0, static_cast<int>(std::lround(corner.y)));
    const int right = std::min(canvas.width, static_cast<int>(std::lround(corner.x + size.x)));
    const int bottom = std::min(canvas.height, static_cast<int>(std::lround(corner.y + size.y)));
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            blend(canvas, x, y, color);
}

void draw_text(Canvas &canvas, std::string_view text, Vec2 corner, int scale, Color color) {
    const int left = static_cast<int>(std::lround(corner.x)), top = static_cast<int>(std::lround(corner.y));
    for (std::size_t column = 0; column < text.size(); ++column) {
        const kinetra::Glyph rows = kinetra::glyph(text[column]);
        for (int row = 0; row < glyph_height; ++row)
            for (int bit = 0; bit < 5; ++bit)
                if (rows[static_cast<std::size_t>(row)] & (1 << (4 - bit)))
                    fill(canvas,
                         {static_cast<float>(left +
                                             (static_cast<int>(column) * glyph_cell_width + bit) * scale),
                          static_cast<float>(top + row * scale)},
                         {static_cast<float>(scale), static_cast<float>(scale)}, color);
    }
}

void draw_ring(Canvas &canvas, Vec2 center, float radius, float thickness, Color color) {
    const int reach = static_cast<int>(std::ceil(radius)) + 1;
    const int cx = static_cast<int>(std::lround(center.x)), cy = static_cast<int>(std::lround(center.y));
    for (int y = cy - reach; y <= cy + reach; ++y)
        for (int x = cx - reach; x <= cx + reach; ++x) {
            const float distance =
                std::hypot(static_cast<float>(x) + 0.5f - center.x, static_cast<float>(y) + 0.5f - center.y);
            // One pixel of falloff on both edges keeps the ring smooth without supersampling.
            const float coverage = std::clamp(radius - distance + 0.5f, 0.f, 1.f) *
                                   std::clamp(distance - (radius - thickness) + 0.5f, 0.f, 1.f);
            if (coverage > 0)
                blend(canvas, x, y, color, coverage);
        }
}

void draw_pointer(Canvas &canvas, Vec2 tip) {
    const int left = static_cast<int>(std::lround(tip.x)), top = static_cast<int>(std::lround(tip.y));
    for (std::size_t row = 0; row < pointer_shape.size(); ++row)
        for (std::size_t column = 0; column < pointer_shape[row].size(); ++column) {
            const char cell = pointer_shape[row][column];
            if (cell == ' ')
                continue;
            fill(canvas,
                 {static_cast<float>(left + static_cast<int>(column) * pointer_scale),
                  static_cast<float>(top + static_cast<int>(row) * pointer_scale)},
                 {static_cast<float>(pointer_scale), static_cast<float>(pointer_scale)},
                 cell == 'X' ? Color{0, 0, 0} : Color{1, 1, 1});
        }
}

int keycap_width(std::string_view label) {
    return text_width(label, keycap_text_scale) + 2 * keycap_padding;
}

void draw_keycap(Canvas &canvas, std::string_view label, Vec2 corner, float strength) {
    const Vec2 size{static_cast<float>(keycap_width(label)), static_cast<float>(keycap_height)};
    constexpr float edge = 2;
    fill(canvas, corner, size, faded(keycap_edge, strength));
    fill(canvas, corner + Vec2{edge, edge}, size - Vec2{2 * edge, 2 * edge}, faded(keycap_face, strength));
    draw_text(canvas, label,
              corner + Vec2{static_cast<float>(keycap_padding),
                            static_cast<float>(keycap_height - glyph_height * keycap_text_scale) / 2},
              keycap_text_scale, faded(keycap_text, strength));
}

void dim(Canvas &canvas, float brightness) {
    const float factor = std::clamp(brightness, 0.f, 1.f);
    for (auto &channel : canvas.rgb)
        channel = static_cast<std::uint8_t>(std::lround(static_cast<float>(channel) * factor));
}
} // namespace capture
