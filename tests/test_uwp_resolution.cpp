// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../xbox/platform/render_resolution.h"
#include "../gpu/shadps4/video_core/renderer_vulkan/scene_resolution.h"
#include <cassert>
#include <cstdio>
int main() {
    using UnchartedResolution::Parse;
    const auto reduced=Parse("");assert(reduced && reduced->width==1280 && reduced->height==720);
    for(const auto text:{"720","720p"}) {
        const auto r=Parse(text);assert(r && r->width==1280 && r->height==720);
    }
    for(const auto text:{"1080","1080p"}) {
        const auto r=Parse(text);assert(r && r->width==1920 && r->height==1080);
    }
    for(const auto text:{"0","720junk","4k","-1","1280x1080"}) assert(!Parse(text));
    const auto preset=Vulkan::SceneResolution::ForPreset(1);
    assert(preset.width==reduced->width && preset.height==reduced->height);
    assert(preset.width*9==preset.height*16);
    assert(preset.width/2==640 && preset.height/2==360);
    const auto unpacked=Vulkan::SceneResolution::Unpack(Vulkan::SceneResolution::Pack(preset));
    assert(unpacked==preset);
    // Verify the coordinate factor consumed by the existing viewport path.
    const double sx=double(preset.width)/1920,sy=double(preset.height)/1080;
    assert(sx==sy && 1920*sx==1280 && 1080*sy==720);
    assert(preset.width*preset.height*9==1920*1080*4);
    std::puts("UWP resolution contract: PASS; 720p default, 1080p fallback, invalid input, scene packing and viewport factor; no GPU execution");
}
