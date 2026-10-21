/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdlib.h>
#include <math.h>
#include "xbox/platform/runtime_pad_uwp.c"
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static UnchartedInput physical;
static int status,capture,vibration_status=1;
static uint8_t large_motor,small_motor;
static unsigned vibrations;
int uncharted_uwp_gamepad(UnchartedInput* out) { *out=physical; return status; }
int uncharted_uwp_vibration(uint8_t large,uint8_t small) { large_motor=large; small_motor=small; ++vibrations; return vibration_status; }
int bbgpu_overlay_captures_input(void) { return capture; }
uintptr_t runtime_lookup(const RuntimeExport* table,size_t count,const char* name) {
    for(size_t i=0;i<count;++i) if(!strcmp(name,table[i].name)) return (uintptr_t)table[i].function;
    return 0;
}
int main(void) {
    typedef int32_t (ABI *Init)(void);
    typedef int32_t (ABI *Open)(int32_t,int32_t,int32_t,const void*);
    typedef int32_t (ABI *Read)(int32_t,UwpPadData*,int32_t);
    typedef int32_t (ABI *Info)(int32_t,UwpControllerInfo*);
    Init init=(Init)runtime_pad_resolve("scePadInit");
    Open open=(Open)runtime_pad_resolve("scePadOpen");
    Read read=(Read)runtime_pad_resolve("scePadRead");
    Info info=(Info)runtime_pad_resolve("scePadGetControllerInformation");
    CHECK(init&&open&&read&&info);
    CHECK(!runtime_pad_resolve("scePadSetMotionSensorState"));
    CHECK(open(1,0,0,NULL)==NOT_INITIALIZED);
    CHECK(init()==0); CHECK(open(2,0,0,NULL)==INVALID_ARG); CHECK(open(1,0,1,NULL)==INVALID_ARG);
    CHECK(open(1,0,0,NULL)==1); CHECK(open(1,0,0,NULL)==ALREADY_OPENED);
    UwpPadData data[2]; memset(data,0xaa,sizeof(data));
    CHECK(read(2,data,1)==INVALID_HANDLE); CHECK(read(1,NULL,1)==INVALID_ARG);
    CHECK(read(1,data,0)==INVALID_ARG); CHECK(read(1,data,65)==INVALID_ARG);
    status=0; CHECK(read(1,data,2)==1); CHECK(!data[0].connected&&!data[0].buttons);
    CHECK(data[0].left_x==128&&data[0].right_y==128);
    for(size_t i=0;i<sizeof(data[1]);++i) CHECK(((unsigned char*)&data[1])[i]==0xaa);
    status=1; physical=(UnchartedInput){.generation=7,.timestamp=12345};
    CHECK(read(1,data,1)==1); CHECK(data[0].connected&&data[0].connected_count==1&&data[0].timestamp==12345);
    static const uint32_t expected[]={0x4000,0x2000,0x8000,0x1000,0x400,0x800,2,4,8,0x100000,0x10,0x20,0x40,0x80};
    for(unsigned i=0;i<14;++i) {
        physical.buttons=1u<<i; CHECK(read(1,data,1)==1); CHECK(data[0].buttons==expected[i]);
    }
    physical.buttons=0; physical.lx=-1; physical.ly=1; physical.rx=1; physical.ry=-1; physical.lt=0.5; physical.rt=1;
    CHECK(read(1,data,1)==1); CHECK(data[0].left_x==0&&data[0].left_y==0&&data[0].right_x==255&&data[0].right_y==255);
    CHECK(data[0].l2==127&&data[0].r2==255&&data[0].buttons==0x300);
    physical.lx=NAN; physical.lt=NAN; physical.rt=-2;
    CHECK(read(1,data,1)==1); CHECK(data[0].left_x==128&&data[0].l2==0&&data[0].r2==0);
    physical.buttons=INPUT_A; capture=1; CHECK(read(1,data,1)==1);
    CHECK(data[0].connected&&data[0].buttons==0&&data[0].right_x==128); capture=0;
    UwpControllerInfo ci; CHECK(info(1,&ci)==0&&ci.connected&&ci.connected_count==1);
    status=0; CHECK(info(1,&ci)==0&&!ci.connected); CHECK(read(1,data,1)==1&&!data[0].connected);
    status=1; CHECK(read(1,data,1)==1&&data[0].connected_count==2);
    physical.generation=8; CHECK(read(1,data,1)==1&&data[0].connected_count==3);
    status=-1; CHECK(read(1,data,1)==HOST_FAILURE&&!data[0].connected);
    uint8_t motors[]={93,201}; CHECK(pad_vibration(1,motors)==0&&large_motor==93&&small_motor==201);
    vibration_status=0; CHECK(pad_vibration(1,motors)==NOT_CONNECTED);
    vibration_status=-1; CHECK(pad_vibration(1,motors)==HOST_FAILURE); CHECK(pad_vibration(1,NULL)==INVALID_ARG);
    uint8_t rgb[]={30,40,50}; CHECK(pad_lightbar(1,rgb)==0&&!memcmp(lightbar,rgb,3));
    CHECK(pad_reset_lightbar(1)==0&&lightbar[0]==0&&lightbar[2]==255);
    vibration_status=1; CHECK(pad_close(1)==0&&large_motor==0&&small_motor==0&&vibrations>=4);
    CHECK(read(1,data,1)==INVALID_HANDLE); CHECK(pad_close(1)==INVALID_HANDLE);
    CHECK(boot.opens==5&&boot.last_user==1&&boot.last_open==ALREADY_OPENED);
    CHECK(boot.host_absent>=2&&boot.host_errors==1&&boot.read_errors==4);
    FILE* log=tmpfile();CHECK(log!=NULL);
    pthread_mutex_lock(&lock);runtime_pad_boot_snapshot(log);pthread_mutex_unlock(&lock);
    rewind(log);char line[1024];CHECK(fgets(line,sizeof(line),log)&&strstr(line,"service busy; snapshot skipped"));
    fclose(log);log=tmpfile();CHECK(log!=NULL);runtime_pad_boot_snapshot(log);rewind(log);
    CHECK(fgets(line,sizeof(line),log)&&strstr(line,"opened=0")&&strstr(line,"host_errors=1"));fclose(log);
    printf("Native pad contract: PASS checks=%u; ABI, mappings, errors, hotplug and rumble routing\n",checks);
    return 0;
}
