// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace UnchartedDepthWriteback {
enum class Mode { Off, Stages };
constexpr std::optional<Mode> Parse(std::string_view text) {
    if (text.empty() || text == "stages") return Mode::Stages;
    if (text == "off") return Mode::Off;
    return {};
}
constexpr bool Contains(std::uint64_t capacity, std::uint64_t offset, std::uint64_t bytes) {
    return bytes && offset <= capacity && bytes <= capacity - offset;
}
// Exact final byte extent for an uncompressed 32-bit depth plane. Padding after
// the final row/slice is not copied. Nullopt rejects empty/invalid/overflowing input.
constexpr std::optional<std::uint64_t> CopyBytes32(std::uint64_t width,
        std::uint64_t height, std::uint64_t depth, std::uint64_t layers,
        std::uint64_t row_length, std::uint64_t image_height) {
    if (!width || !height || !depth || !layers) return {};
    if (!row_length) row_length = width;
    if (!image_height) image_height = height;
    if (row_length < width || image_height < height) return {};
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    if (depth > max / layers) return {};
    const auto slices = depth * layers;
    if (slices - 1 > max / image_height) return {};
    const auto preceding_rows = (slices - 1) * image_height;
    if (height - 1 > max - preceding_rows) return {};
    const auto rows = preceding_rows + height - 1;
    if (rows > (max - width) / row_length) return {};
    const auto texels = rows * row_length + width;
    if (texels > max / 4) return {};
    return texels * 4;
}
}
