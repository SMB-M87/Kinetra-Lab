#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace capture {
/// Writes an eight-bit RGB image as a PNG file, replacing `path` if it exists.
///
/// `rgb` holds `width * height` pixels of three bytes, row by row from the top, without
/// padding. The image data is one zlib stream at the default compression level, with the
/// filter that suits each row (none or up), so flat-colored frames stay small.
/// Throws std::invalid_argument when a dimension is zero or `rgb` has the wrong size, and
/// std::runtime_error when the file cannot be written or zlib fails.
void write_png(const std::filesystem::path &path, std::uint32_t width, std::uint32_t height,
               std::span<const std::uint8_t> rgb);
} // namespace capture
