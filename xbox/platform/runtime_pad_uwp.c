/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native Xbox guest pad service. Guest layouts/exports follow src/runtime_pad.c;
 * no SDL, keyboard emulation or fabricated connection/sensor readings.
 */
#include "runtime.h"
#include "gpu/bbgpu.h"
#include "input.h"
#include "pad_abi.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define INVALID_ARG ((int32_t)0x80920001)
#define INVALID_HANDLE ((int32_t)0x80920003)
#define ALREADY_OPENED ((int32_t)0x80920004)
#define NOT_INITIALIZED ((int32_t)0x80920005)
#define NOT_CONNECTED ((int32_t)0x80920007)
#define HOST_FAILURE ((int32_t)0x809200ff)
_Static_assert(sizeof(UwpPadData)==120,"guest PadData size");
_Static_assert(offsetof(UwpPadData,touches)==60,"guest touches offset");
_Static_assert(offsetof(UwpPadData,timestamp)==80,"guest timestamp offset");
_Static_assert(sizeof(UwpControllerInfo)==28,"guest ControllerInfo size");
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static int initialized,opened,connected;
static uint8_t connections;
static uint32_t generation;
static size_t reads;
static uint8_t axis(double value) {
    if(!isfinite(value)) return 128;
    if(value<=-1) return 0;
    if(value>=1) return 255;
    return (uint8_t)((value+1)*127.5+0.5);
}
static uint8_t trigger(double value) {
    if(!isfinite(value)||value<=0) return 0;
    if(value>=1) return 255;
    return (uint8_t)(value*255);
}
static int sample(UwpPadData* data) {
    memset(data,0,sizeof(*data));
    data->left_x=data->left_y=data->right_x=data->right_y=128;
    data->orientation[3]=1.0f;
    UnchartedInput input={0};
    int result=uncharted_uwp_gamepad(&input);
    if(result<=0) { connected=0; return result; }
    if(!connected||input.generation!=generation) {
        if(connections<255) ++connections;
        generation=input.generation;
    }
    connected=1;
    data->connected=1; data->connected_count=connections; data->timestamp=input.timestamp;
    data->left_x=axis(input.lx); data->left_y=axis(-input.ly);
    data->right_x=axis(input.rx); data->right_y=axis(-input.ry);
    data->l2=trigger(input.lt); data->r2=trigger(input.rt);
    static const struct { uint32_t host,guest; } mapping[]={
        {INPUT_A,0x4000},{INPUT_B,0x2000},{INPUT_X,0x8000},{INPUT_Y,0x1000},
        {INPUT_LB,0x400},{INPUT_RB,0x800},{INPUT_LS,2},{INPUT_RS,4},
        {INPUT_MENU,8},{INPUT_VIEW,0x100000},
        {INPUT_UP,0x10},{INPUT_RIGHT,0x20},{INPUT_DOWN,0x40},{INPUT_LEFT,0x80}};
    for(size_t i=0;i<sizeof(mapping)/sizeof(*mapping);++i)
        if(input.buttons&mapping[i].host) data->buttons|=mapping[i].guest;
    if(data->l2>30) data->buttons|=0x100;
    if(data->r2>30) data->buttons|=0x200;
    // Do not leak gameplay presses through an open renderer overlay.
    if(bbgpu_overlay_captures_input()) {
        data->buttons=0; data->left_x=data->left_y=data->right_x=data->right_y=128;
        data->l2=data->r2=0;
    }
    return 1;
}
static ABI int32_t pad_init(void) {
    pthread_mutex_lock(&lock); initialized=1; pthread_mutex_unlock(&lock); return 0;
}
static ABI int32_t pad_open(int32_t user,int32_t type,int32_t index,const void* param) {
    (void)param;
    pthread_mutex_lock(&lock);
    int32_t result=!initialized ? NOT_INITIALIZED : user!=1||(type!=0&&type!=2)||index ? INVALID_ARG : opened ? ALREADY_OPENED : 1;
    if(result==1) { opened=1; connected=0; connections=0; }
    pthread_mutex_unlock(&lock); return result;
}
static ABI int32_t pad_close(int32_t handle) {
    pthread_mutex_lock(&lock);
    int32_t result=handle!=1||!opened ? INVALID_HANDLE : 0;
    if(!result) { uncharted_uwp_vibration(0,0); opened=0; }
    pthread_mutex_unlock(&lock); return result;
}
static ABI int32_t pad_read_state(int32_t handle,UwpPadData* data) {
    pthread_mutex_lock(&lock);
    int32_t result=handle!=1||!opened ? INVALID_HANDLE : !data ? INVALID_ARG : 0;
    if(!result) { if(sample(data)<0) result=HOST_FAILURE; ++reads; }
    pthread_mutex_unlock(&lock); return result;
}
static ABI int32_t pad_read(int32_t handle,UwpPadData* data,int32_t count) {
    if(count<1||count>64) return INVALID_ARG;
    int32_t result=pad_read_state(handle,data); return result ? result : 1;
}
static ABI int32_t pad_info(int32_t handle,UwpControllerInfo* info) {
    pthread_mutex_lock(&lock);
    int32_t result=handle!=1||!opened ? INVALID_HANDLE : !info ? INVALID_ARG : 0;
    if(!result) {
        UwpPadData data; memset(info,0,sizeof(*info));
        if(sample(&data)<0) result=HOST_FAILURE;
        info->pixel_density=44.86f; info->resolution_x=1920; info->resolution_y=943;
        info->dead_zone_left=info->dead_zone_right=2;
        info->connected=data.connected; info->connected_count=data.connected_count;
    }
    pthread_mutex_unlock(&lock); return result;
}
static ABI int32_t pad_vibration(int32_t handle,const uint8_t* motors) {
    pthread_mutex_lock(&lock);
    int32_t result=handle!=1||!opened ? INVALID_HANDLE : !motors ? INVALID_ARG : 0;
    if(!result) { int host=uncharted_uwp_vibration(motors[0],motors[1]); result=host>0 ? 0 : host==0 ? NOT_CONNECTED : HOST_FAILURE; }
    pthread_mutex_unlock(&lock); return result;
}
// Xbox controllers do not expose a DS4 RGB lightbar. Retain the requested virtual state.
static uint8_t lightbar[3]={0,0,255};
static ABI int32_t pad_lightbar(int32_t handle,const uint8_t* rgb) {
    pthread_mutex_lock(&lock);
    int32_t result=handle!=1||!opened ? INVALID_HANDLE : !rgb ? INVALID_ARG : 0;
    if(!result) memcpy(lightbar,rgb,3);
    pthread_mutex_unlock(&lock); return result;
}
static ABI int32_t pad_reset_lightbar(int32_t handle) { const uint8_t rgb[3]={0,0,255}; return pad_lightbar(handle,rgb); }
static const RuntimeExport exports[]={
    {"scePadInit",pad_init},{"scePadOpen",pad_open},{"scePadClose",pad_close},
    {"scePadReadState",pad_read_state},{"scePadRead",pad_read},
    {"scePadGetControllerInformation",pad_info},{"scePadSetVibration",pad_vibration},
    {"scePadSetLightBar",pad_lightbar},{"scePadResetLightBar",pad_reset_lightbar}};
uintptr_t runtime_pad_resolve(const char* name) { return RUNTIME_LOOKUP(exports,name); }
void runtime_pad_report(void) {
    pthread_mutex_lock(&lock);
    printf("Runtime: native Xbox pad reads=%zu connections=%u connected=%d; no motion/touch sensors\n",reads,connections,connected);
    pthread_mutex_unlock(&lock);
}
