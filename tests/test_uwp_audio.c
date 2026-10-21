/* Real guest AudioOut ABI with a deterministic host-device mock. */
#include "xbox/platform/runtime_audio_uwp.c"
#include <math.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"Audio FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
struct UnchartedUwpAudioStream { int channels,is_float; };
static struct UnchartedUwpAudioStream device;
static int device_ok=1,submit_ok=1,last_size,host_channels=2;
static unsigned char last_pcm[2048*8*4];
int uncharted_uwp_audio_init(void) { return device_ok; }
int uncharted_uwp_audio_channels(void) { return host_channels; }
UnchartedUwpAudioStream* uncharted_uwp_audio_open(int channels,int is_float) {
    device=(struct UnchartedUwpAudioStream){channels,is_float}; return &device;
}
void uncharted_uwp_audio_close(UnchartedUwpAudioStream* stream) { (void)stream; }
int uncharted_uwp_audio_queued(UnchartedUwpAudioStream* stream) { (void)stream; return 16384; }
int uncharted_uwp_audio_put(UnchartedUwpAudioStream* stream,const void* data,int size) {
    (void)stream; if(!submit_ok) return -1;
    CHECK(size>0 && (size_t)size<=sizeof(last_pcm)); memcpy(last_pcm,data,(size_t)size);last_size=size; return 1;
}
uintptr_t runtime_lookup(const RuntimeExport* table,size_t count,const char* name) {
    for(size_t i=0;i<count;++i) if(!strcmp(name,table[i].name)) return (uintptr_t)table[i].function;
    return 0;
}
#ifdef _WIN32
void compat_sleep_ns(uint64_t ns) { Sleep((DWORD)((ns+999999)/1000000)); }
#endif
int main(int argc,char**argv) {
    typedef int32_t (ABI *Init)(void);
    Init init=(Init)runtime_audio_resolve("sceAudioOutInit"); CHECK(init!=NULL);
    if(argc>1 && !strcmp(argv[1],"--device-failure")) {
        device_ok=0; CHECK(init()==(int32_t)0x80260011); CHECK(!initialized);
        puts("Audio device failure contract: PASS (no silent fallback)"); return 0;
    }
    CHECK(audio_open(1,0,0,256,48000,1)==ERR_NOT_INIT);
    CHECK(init()==0); CHECK(init()==ERR_ALREADY_INIT);
    CHECK(audio_open(1,0,0,0,48000,1)==ERR_INVALID_SIZE);
    CHECK(audio_open(1,0,0,257,48000,1)==ERR_INVALID_SIZE);
    CHECK(audio_open(1,0,0,256,44100,1)==ERR_INVALID_FREQ);
    CHECK(audio_open(1,0,0,256,48000,8)==ERR_INVALID_FORMAT);
    CHECK(audio_open(1,20,0,256,48000,1)==ERR_INVALID_TYPE);
    int h=audio_open(1,0,0,256,48000,1); CHECK(h==0x20000000);
    CHECK(audio_state(h|0x10000,NULL)==ERR_INVALID_PORT);
    CHECK(audio_state(h,NULL)==ERR_INVALID_POINTER);
    CHECK(audio_volume(h,1,NULL)==ERR_INVALID_POINTER);
    int32_t volume[8]={16384,40000}; CHECK(audio_volume(h,3,volume)==ERR_INVALID_VOLUME);
    CHECK(ports[0].volume[0]==32768); volume[1]=16384; CHECK(audio_volume(h,3,volume)==0);
    CHECK(audio_volume(h,256,volume)==(int32_t)0x80260013);
    int16_t samples[512]; for(int i=0;i<512;++i) samples[i]=(int16_t)(i&1 ? -12000 : 12000);
    CHECK(audio_output(h,samples)==512); CHECK(last_size==1024);
    CHECK(((int16_t*)last_pcm)[0]==6000 && ((int16_t*)last_pcm)[1]==-6000);
    uint64_t timestamp=0; CHECK(audio_last_time(h,&timestamp)==0 && timestamp>0);
    CHECK(audio_last_time(h,NULL)==ERR_INVALID_POINTER);
    uint64_t begin=now_ns();for(int i=0;i<10;++i) CHECK(audio_output(h,samples)==512);
    CHECK(now_ns()-begin>=30000000); /* output calls cannot spin ahead of the mixer */
    submit_ok=0;CHECK(audio_output(h,samples)==(int32_t)0x80260011);submit_ok=1;
    CHECK(audio_close(h)==0);CHECK(audio_output(h,samples)==ERR_NOT_OPENED);
    static float surround[2048*8];
    for(int format=0;format<8;++format) {
        h=audio_open(1,0,0,2048,48000,(uint32_t)format);CHECK(h>0);
        CHECK(audio_output(h,surround)==2048*ports[0].channels);
        CHECK(last_size==2048*device.channels*(device.is_float?4:2));CHECK(audio_close(h)==0);
    }
    h=audio_open(1,0,0,256,48000,5);CHECK(h>0);surround[0]=0.2f;surround[2]=0.4f;
    CHECK(audio_output(h,surround)==2048);
    CHECK(fabsf(((float*)last_pcm)[0]-(0.2f+0.4f*0.7071f))<0.0001f);
    CHECK(fabsf(((float*)last_pcm)[1]-0.4f*0.7071f)<0.0001f); CHECK(audio_close(h)==0);
    host_channels=8;h=audio_open(1,0,0,256,48000,5); CHECK(h>0);
    memset(surround,0,sizeof(surround));surround[4]=0.3f;surround[6]=0.6f;
    CHECK(audio_output(h,surround)==2048); CHECK(((float*)last_pcm)[4]==0.6f && ((float*)last_pcm)[6]==0.3f);
    CHECK(audio_outputs(NULL,1)==ERR_INVALID_POINTER);CHECK(audio_outputs(&(OutputParam){h,NULL},0)==ERR_PORT_FULL);
    CHECK(audio_close(h)==0);
    printf("AudioOut contract: PASS checks=%u; ABI, PCM, downmix, volume, pacing and errors\n",checks);
}
