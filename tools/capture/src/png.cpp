#include "png.h"

#include <zlib.h>

#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace capture {
namespace {
/// PNG filter types for a scanline (PNG specification, section 9.2).
constexpr std::uint8_t filter_none = 0, filter_up = 2;

void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

/// Appends one chunk: length, type, data and the CRC over type and data.
void append_chunk(std::vector<std::uint8_t> &out, const char (&type)[5], std::span<const std::uint8_t> data) {
    append_u32(out, static_cast<std::uint32_t>(data.size()));
    const auto type_start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    const auto crc = crc32(0, out.data() + type_start, static_cast<uInt>(out.size() - type_start));
    append_u32(out, static_cast<std::uint32_t>(crc));
}

/// The filtered scanlines: each row starts with its filter byte. A row equal or close to the
/// one above compresses far better as differences ("up"); the cheaper of the two is chosen
/// by the sum of absolute byte values, the usual heuristic.
std::vector<std::uint8_t> filtered_rows(std::uint32_t width, std::uint32_t height,
                                        std::span<const std::uint8_t> rgb) {
    const std::size_t stride = std::size_t(width) * 3;
    std::vector<std::uint8_t> rows;
    rows.reserve((stride + 1) * height);
    std::vector<std::uint8_t> up(stride);
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto *row = rgb.data() + y * stride;
        if (y == 0) {
            rows.push_back(filter_none);
            rows.insert(rows.end(), row, row + stride);
            continue;
        }
        const auto *above = row - stride;
        unsigned long none_cost = 0, up_cost = 0;
        for (std::size_t i = 0; i < stride; ++i) {
            up[i] = static_cast<std::uint8_t>(row[i] - above[i]);
            none_cost += row[i] < 128 ? row[i] : 256u - row[i];
            up_cost += up[i] < 128 ? up[i] : 256u - up[i];
        }
        if (up_cost < none_cost) {
            rows.push_back(filter_up);
            rows.insert(rows.end(), up.begin(), up.end());
        } else {
            rows.push_back(filter_none);
            rows.insert(rows.end(), row, row + stride);
        }
    }
    return rows;
}
} // namespace

void write_png(const std::filesystem::path &path, std::uint32_t width, std::uint32_t height,
               std::span<const std::uint8_t> rgb) {
    if (width == 0 || height == 0)
        throw std::invalid_argument("PNG dimensions must be positive");
    if (rgb.size() != std::size_t(width) * height * 3)
        throw std::invalid_argument("PNG pixel data does not match " + std::to_string(width) + " x " +
                                    std::to_string(height) + " RGB");
    const auto rows = filtered_rows(width, height, rgb);
    uLongf compressed_size = compressBound(static_cast<uLong>(rows.size()));
    std::vector<std::uint8_t> compressed(compressed_size);
    if (compress2(compressed.data(), &compressed_size, rows.data(), static_cast<uLong>(rows.size()),
                  Z_DEFAULT_COMPRESSION) != Z_OK)
        throw std::runtime_error("zlib could not compress " + path.string());
    compressed.resize(compressed_size);

    std::vector<std::uint8_t> file{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    std::vector<std::uint8_t> header;
    append_u32(header, width);
    append_u32(header, height);
    // Bit depth 8, color type 2 (RGB), deflate, adaptive filtering, no interlace.
    header.insert(header.end(), {8, 2, 0, 0, 0});
    append_chunk(file, "IHDR", header);
    append_chunk(file, "IDAT", compressed);
    append_chunk(file, "IEND", {});

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
    if (!out)
        throw std::runtime_error("Could not write " + path.string());
}
} // namespace capture
