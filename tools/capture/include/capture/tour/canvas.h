#pragma once
// A video frame as pixels, and what the tour draws on top of a frame it read back: the
// pointer, click rings, keycaps and captions. Software drawing, so the applications do
// not know they are filmed.
#include <kinetra/runtime/engine.h>
#include <kinetra/simulation/math.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace capture {
/// One frame: eight-bit RGB, rows top-down, `width` × `height` pixels.
///
/// Drawing functions take positions in pixels with the origin at the top-left corner, clip
/// to the frame and blend with the color's alpha.
struct Canvas {
    int width{}, height{};
    /// width × height × 3 bytes.
    std::vector<std::uint8_t> rgb;

    /// A black frame. Throws std::invalid_argument for a size that is not positive.
    Canvas(int width, int height);
    /// The color of pixel (x, y), each channel in [0, 1]. Throws std::out_of_range outside
    /// the frame.
    kinetra::Color pixel(int x, int y) const;
};

/// Side of one font pixel of text drawn at `scale`, and the size of one character cell
/// (six by seven font pixels, the last column being the gap to the next character).
inline constexpr int glyph_cell_width = 6, glyph_height = 7;

/// Width in pixels of `text` drawn at `scale`: its cells less the gap after the last.
int text_width(std::string_view text, int scale);

/// Blends a filled rectangle from its top-left `corner`, `size` pixels large.
void fill(Canvas &canvas, kinetra::Vec2 corner, kinetra::Vec2 size, kinetra::Color color);

/// Draws `text` in the engine's bitmap font with its top-left corner at `corner`; every font
/// pixel is a square of `scale` pixels.
void draw_text(Canvas &canvas, std::string_view text, kinetra::Vec2 corner, int scale, kinetra::Color color);

/// Blends a ring around `center` with outer radius `radius` and the given `thickness`.
void draw_ring(Canvas &canvas, kinetra::Vec2 center, float radius, float thickness, kinetra::Color color);

/// Size of the arrow pointer in pixels.
inline constexpr int pointer_width = 24, pointer_height = 38;

/// Draws the arrow pointer, white with a black outline, with its tip at `tip`.
void draw_pointer(Canvas &canvas, kinetra::Vec2 tip);

/// Height of a keycap and the space between its edge and its label, in pixels.
inline constexpr int keycap_height = 34, keycap_padding = 10;

/// Width of the keycap that shows `label`.
int keycap_width(std::string_view label);

/// Draws a keycap with `label` and its top-left corner at `corner`; `strength` in [0, 1]
/// fades it, for a key that was released a moment ago.
void draw_keycap(Canvas &canvas, std::string_view label, kinetra::Vec2 corner, float strength);

/// Scales every pixel's brightness by `brightness` in [0, 1], for a fade from or to black.
void dim(Canvas &canvas, float brightness);
} // namespace capture
