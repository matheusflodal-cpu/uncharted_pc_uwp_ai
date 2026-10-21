// SPDX-License-Identifier: GPL-2.0-or-later
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include "pad_abi.h"
extern "C" {
#include "runtime.h"
}
int UnchartedInputProbe() {
    using Init=int32_t (ABI*)();
    using Open=int32_t (ABI*)(int32_t,int32_t,int32_t,const void*);
    using Read=int32_t (ABI*)(int32_t,UwpPadData*,int32_t);
    using Close=int32_t (ABI*)(int32_t);
    using Vibration=int32_t (ABI*)(int32_t,const uint8_t*);
    auto init=reinterpret_cast<Init>(runtime_pad_resolve("hv1luiJrqQM#A#B"));
    auto open=reinterpret_cast<Open>(runtime_pad_resolve("xk0AcarP3V4#A#B"));
    auto read=reinterpret_cast<Read>(runtime_pad_resolve("q1cHNfGycLI#A#B"));
    auto close=reinterpret_cast<Close>(runtime_pad_resolve("6ncge5+l5Qs#A#B"));
    auto rumble=reinterpret_cast<Vibration>(runtime_pad_resolve("yFVnOdGxvZY#A#B"));
    if(!init||!open||!read||!close||!rumble||init()) return 1;
    int handle=open(1,0,0,nullptr); if(handle<0) return 2;
    std::puts("Native Xbox input probe: 20 seconds; press A/B/X/Y, rotate both sticks, fully press LT/RT. Y requests a brief vibration.");
    uint32_t seen=0;
    uint8_t lo[4]={255,255,255,255},hi[4]={0,0,0,0},triggers[2]={0,0};
    unsigned samples=0,errors=0;
    bool requested=false,vibrating=false;
    ULONGLONG stop_rumble=0,next_log=0,start=GetTickCount64();
    while(GetTickCount64()-start<20000) {
        if(vibrating&&GetTickCount64()>=stop_rumble) { const uint8_t motors[]={0,0}; rumble(handle,motors); vibrating=false; }
        UwpPadData data{};
        int result=read(handle,&data,1);
        if(result<0) { ++errors; Sleep(20); continue; }
        if(data.connected) {
            ++samples; seen|=data.buttons;
            const uint8_t values[]={data.left_x,data.left_y,data.right_x,data.right_y};
            for(int i=0;i<4;++i) { lo[i]=std::min(lo[i],values[i]); hi[i]=std::max(hi[i],values[i]); }
            triggers[0]=std::max(triggers[0],data.l2); triggers[1]=std::max(triggers[1],data.r2);
            if(!requested&&(data.buttons&0x1000)) {
                const uint8_t motors[]={80,80};
                int status=rumble(handle,motors);
                std::printf("Native rumble request: result=0x%x (physical feedback must be confirmed by player)\n",unsigned(status));
                requested=true; vibrating=status==0; stop_rumble=GetTickCount64()+200;
            }
        }
        if(GetTickCount64()>=next_log) {
            std::printf("Native pad: connected=%u buttons=0x%x LS=%u,%u RS=%u,%u LT=%u RT=%u\n",
                unsigned(data.connected),data.buttons,data.left_x,data.left_y,data.right_x,data.right_y,data.l2,data.r2);
            next_log=GetTickCount64()+1000;
        }
        Sleep(20);
    }
    close(handle);
    bool sticks=true;
    for(int i=0;i<4;++i) sticks=sticks&&lo[i]<96&&hi[i]>160;
    const bool pass=samples&&errors==0&&(seen&0xf000)==0xf000&&sticks&&triggers[0]>200&&triggers[1]>200;
    std::printf("Native input probe: %s connected_samples=%u errors=%u buttons_seen=0x%x LS=%u..%u,%u..%u RS=%u..%u,%u..%u triggers=%u,%u\n",
        pass ? "PASS" : "INCOMPLETE",samples,errors,seen,lo[0],hi[0],lo[1],hi[1],lo[2],hi[2],lo[3],hi[3],triggers[0],triggers[1]);
    return pass ? 0 : 22;
}
