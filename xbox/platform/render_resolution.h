// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
namespace UnchartedResolution {
struct Setting { std::uint32_t width,height; };
constexpr std::optional<Setting> Parse(std::string_view value) {
    if(value.empty() || value=="720p" || value=="720") return Setting{1280,720};
    if(value=="1080p" || value=="1080") return Setting{1920,1080};
    return {};
}
}
