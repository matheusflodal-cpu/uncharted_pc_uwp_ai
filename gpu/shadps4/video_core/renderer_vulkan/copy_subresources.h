// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>

namespace Vulkan::CopySubresources {
// Use subtraction after checking the bases: base+count can overflow uint32_t.
constexpr std::uint32_t LayerCount(std::uint32_t src_total, std::uint32_t dst_total,
                                 std::uint32_t src_base, std::uint32_t dst_base,
                                 std::uint32_t src_wanted, std::uint32_t dst_wanted) {
    if (src_base >= src_total || dst_base >= dst_total) return 0;
    return std::min({src_total-src_base, dst_total-dst_base, src_wanted, dst_wanted});
}
struct Image {
    std::uint32_t width, height, levels, layers;
};
struct View {
    std::uint32_t level, layer, layers;
};
struct Region {
    View src, dst;
    std::uint32_t width, height;
};
// A color resolve/copy operates on the selected attachment mip and layers,
// not on every layer of the underlying cached image.
constexpr std::optional<Region> Attachment(const Image& src, const Image& dst,
                                          View source, View dest) {
    if (!src.width || !src.height || !dst.width || !dst.height ||
        source.level >= src.levels || dest.level >= dst.levels ||
        source.level >= 32 || dest.level >= 32) return {};
    const auto count=LayerCount(src.layers,dst.layers,source.layer,dest.layer,
                                source.layers,dest.layers);
    if (!count) return {};
    source.layers=dest.layers=count;
    return Region{source,dest,
        std::min(std::max(src.width>>source.level,1u),std::max(dst.width>>dest.level,1u)),
        std::min(std::max(src.height>>source.level,1u),std::max(dst.height>>dest.level,1u))};
}
} // namespace Vulkan::CopySubresources
