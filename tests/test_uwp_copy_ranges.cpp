// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../gpu/shadps4/video_core/renderer_vulkan/copy_subresources.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>
using namespace Vulkan::CopySubresources;
int main() {
    // Previous ResolveImage used all four image layers starting at layer 3:
    // 3+4 > 4. It also copied mip zero at full size instead of the selected mip.
    const Image src{128,64,4,4},dst{64,32,3,4};
    const auto p=Attachment(src,dst,{2,3,1},{1,2,1});
    assert(p && p->src.level==2 && p->dst.level==1);
    assert(p->src.layer==3 && p->dst.layer==2 && p->src.layers==1 && p->dst.layers==1);
    assert(p->width==32 && p->height==16);
    // Previous CopyMip: min(2,3) layers at destination slice 2 ran past layer 3.
    assert(LayerCount(2,3,0,2,2,2)==1);
    assert(LayerCount(4,4,0,0,4,4)==4);
    assert(!Attachment(src,dst,{4,0,1},{0,0,1}));
    assert(!Attachment(src,dst,{0,4,1},{0,0,1}));
    assert(!Attachment(src,dst,{0,0,0},{0,0,1}));
    assert(!Attachment({0,64,4,4},dst,{0,0,1},{0,0,1}));
    assert(!Attachment({1,1,33,1},{1,1,33,1},{32,0,1},{0,0,1}));
    const auto tiny=Attachment({1,1,4,1},{1,1,4,1},{3,0,1},{3,0,1});
    assert(tiny && tiny->width==1 && tiny->height==1);
    const auto max=std::numeric_limits<std::uint32_t>::max();
    assert(LayerCount(max,max,max-1,max-1,max,max)==1);
    assert(!LayerCount(max,max,max,0,1,1));
    unsigned cases=0;
    // Apply the production range to tagged array slices. Independently check
    // which source/destination pairs exist, including unchanged neighbours.
    for(unsigned st=1;st<=6;++st) for(unsigned dt=1;dt<=6;++dt)
    for(unsigned sb=0;sb<=st;++sb) for(unsigned db=0;db<=dt;++db)
    for(unsigned sw=0;sw<=7;++sw) for(unsigned dw=0;dw<=7;++dw) {
        const auto count=LayerCount(st,dt,sb,db,sw,dw);
        std::array<unsigned,8> written{};
        for(unsigned i=0;i<count;++i) {
            assert(sb+i<st && db+i<dt && i<sw && i<dw);
            written[db+i]=100+sb+i;
        }
        for(unsigned layer=0;layer<dt;++layer) {
            const bool selected=layer>=db && layer-db<sw && layer-db<dw &&
                                std::uint64_t(sb)+layer-db<st;
            assert(written[layer]==(selected?100+sb+layer-db:0));
        }
        ++cases;
    }
    std::printf("Image copy range contract: PASS cases=%u; selected mips/layers, offsets, neighbour preservation and overflow; no GPU execution\n",cases);
}
