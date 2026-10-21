// SPDX-License-Identifier: GPL-2.0-or-later
#include <windows.h>
#include <cmath>
#include <cstdio>
extern "C" {
#include "runtime.h"
}
int UnchartedAudioProbe() {
    using Init=int32_t (ABI*)();
    using Open=int32_t (ABI*)(int32_t,int32_t,int32_t,uint32_t,uint32_t,uint32_t);
    using Output=int32_t (ABI*)(int32_t,const void*);
    using Close=int32_t (ABI*)(int32_t);
    auto init=reinterpret_cast<Init>(runtime_audio_resolve("JfEPXVxhFqA#G#H"));
    auto open=reinterpret_cast<Open>(runtime_audio_resolve("ekNvsT22rsY#G#H"));
    auto output=reinterpret_cast<Output>(runtime_audio_resolve("QOQtbeDqsT4#G#H"));
    auto close=reinterpret_cast<Close>(runtime_audio_resolve("s1--uE9mBFw#G#H"));
    if(!init||!open||!output||!close) return 1;
    auto result=init();if(result<0) {std::printf("Audio probe init: 0x%x\n",unsigned(result));return 2;}
    const int handle=open(1,0,0,256,48000,4);
    if(handle<0) {std::printf("Audio probe open: 0x%x\n",unsigned(handle));return 3;}
    unsigned buffers=0;
    for(unsigned frame=0;frame<96000;frame+=256) {
        float pcm[512];
        for(unsigned i=0;i<256;++i) {
            const unsigned t=frame+i;
            const float sample=0.1f*std::sin(float(t)*6.2831853f*440.0f/48000.0f);
            pcm[i*2]=t<48000 ? sample : 0; pcm[i*2+1]=t>=48000 ? sample : 0;
        }
        if(output(handle,pcm)!=512) {close(handle);std::puts("Audio probe output failed");return 4;}
        ++buffers;
    }
    Sleep(150);
    result=close(handle);
    std::printf("Native Xbox audio probe: %s buffers=%u; physical sound must be confirmed by player\n",result ? "FAIL" : "PASS",buffers);
    runtime_audio_report();return result ? 5 : 0;
}
