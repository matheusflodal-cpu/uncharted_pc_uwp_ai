// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../xbox/platform/depth_writeback.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace UnchartedDepthWriteback;
int main() {
    assert(Parse("") == Mode::Stages && Parse("stages") == Mode::Stages);
    assert(Parse("off") == Mode::Off && !Parse("skip") && !Parse("stagesjunk"));
    unsigned cases=0;
    for(unsigned w=1;w<=8;++w) for(unsigned h=1;h<=6;++h)
    for(unsigned d=1;d<=2;++d) for(unsigned l=1;l<=3;++l)
    for(unsigned px=0;px<4;++px) for(unsigned py=0;py<4;++py) {
        // Independent oracle: visit every copied texel, retaining row/slice padding.
        const unsigned row=w+px,rows=h+py;
        unsigned end=0;
        for(unsigned layer=0;layer<l;++layer) for(unsigned z=0;z<d;++z)
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
            end=(((layer*d+z)*rows+y)*row+x+1)*4;
        const auto bytes=CopyBytes32(w,h,d,l,row,rows);
        assert(bytes && *bytes==end);
        assert(Contains(end+17,17,*bytes));
        assert(!Contains(end+16,17,*bytes));
        ++cases;
    }
    assert(CopyBytes32(1280,720,1,1,0,0)==1280ull*720*4);
    assert(CopyBytes32(1920,1080,1,1,2048,1088)==(1079ull*2048+1920)*4);
    assert(!CopyBytes32(0,1,1,1,0,0));
    assert(!CopyBytes32(8,8,1,1,7,8));
    assert(!CopyBytes32(8,8,1,1,8,7));
    constexpr auto max=std::numeric_limits<std::uint64_t>::max();
    assert(!CopyBytes32(1,1,max,2,1,1));
    assert(!CopyBytes32(max,1,1,1,0,0));
    assert(!CopyBytes32(1,1,2,1,max,max));
    assert(Contains(max,max-3,3) && !Contains(max,max-3,4));
    assert(!Contains(64,0,0) && !Contains(64,65,1));
    std::printf("Depth writeback range contract: PASS cases=%u; padded 32-bit planes, offsets, overflow and config; no GPU execution\n",cases);
}
