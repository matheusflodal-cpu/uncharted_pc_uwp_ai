/* Actual Uncharted AJM parser and decoders, without game data. */
#include "src/runtime_ajm.c"
#include "uwp_audio_fixture.h"
#include "third_party/LibAtrac9/C/src/utility.h"
#include <math.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"AJM FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
uintptr_t runtime_lookup(const RuntimeExport* table,size_t count,const char* name) {
    for(size_t i=0;i<count;++i) if(!strcmp(name,table[i].name)) return (uintptr_t)table[i].function;
    return 0;
}
int main(void) {
    typedef int32_t (ABI *Init)(int64_t,uint32_t*);
    Init init=(Init)runtime_ajm_resolve("sceAjmInitialize");CHECK(init!=NULL);
    uint32_t ctx=0,other=0,instance=0,batch=0;
    CHECK(init(1,&ctx)==ERR_INVALID_PARAMETER);CHECK(init(0,NULL)==ERR_INVALID_PARAMETER);
    CHECK(init(0,&ctx)==0); CHECK(init(0,&other)==0);
    CHECK(ajm_instance_create(ctx,0,1,&instance)==ERR_NOT_REGISTERED);
    CHECK(ajm_module_register(ctx,0,0)==0);CHECK(ajm_module_register(ctx,0,0)==ERR_ALREADY_REGISTERED);
    CHECK(ajm_instance_create(ctx,0,1|((uint64_t)FORMAT_FLOAT<<7),&instance)==0);
    CHECK(ajm_instance_codec(instance)==0);
    unsigned char jobs[256]={0};float pcm[1152*2*3];memset(pcm,0,sizeof(pcm));
    struct {SidebandResult result;SidebandStream stream;SidebandFormat format;} side={0};
    unsigned char* end=job_run(jobs,instance,(UINT64_C(1)<<47)|(UINT64_C(1)<<46)|(UINT64_C(1)<<12),
        (void*)mp3_fixture,sizeof(mp3_fixture),pcm,sizeof(pcm),&side,sizeof(side),NULL);
    CHECK(ajm_batch_start(ctx,jobs,(uint32_t)(end-jobs),0,NULL,&batch)==0);
    CHECK(side.result.result==0);CHECK(side.stream.input_consumed==sizeof(mp3_fixture));
    CHECK(side.stream.output_written>0 && (unsigned)side.stream.output_written<=sizeof(pcm));
    CHECK(side.format.channels==2 && side.format.sample_rate==48000 && side.format.encoding==FORMAT_FLOAT);
    double energy=0;for(int i=0;i<side.stream.output_written/4;++i) {CHECK(isfinite(pcm[i]));energy+=(double)pcm[i]*pcm[i];}
    CHECK(energy>0.01);CHECK(ajm_batch_wait(other,batch,0,NULL)==ERR_INVALID_BATCH);
    CHECK(ajm_batch_wait(ctx,batch,0,NULL)==0);CHECK(ajm_batch_wait(ctx,batch,0,NULL)==ERR_INVALID_BATCH);
    CHECK(ajm_instance_destroy(ctx,instance)==0);
    CHECK(ajm_module_register(ctx,1,0)==0);CHECK(ajm_instance_create(ctx,1,1,&instance)==0);
    unsigned char config[8]={0xfe,0x74,0x1f,0xe0};SidebandResult result={0};
    end=job_control(jobs,instance,UINT64_C(1)<<14,config,sizeof(config),&result,sizeof(result),NULL);
    CHECK(ajm_batch_start(ctx,jobs,(uint32_t)(end-jobs),0,NULL,&batch)==0);
    Instance* at9=&contexts[ctx]->instances[instance & MAX_INSTANCES];
    CHECK(result.result==0 && at9->initialized);CHECK(at9->info.channels==2 && at9->info.samplingRate==48000);
    CHECK(at9->info.frameSamples==256 && at9->info.superframeSize==256);
    CHECK(ajm_batch_wait(ctx,batch,0,NULL)==0);
    CHECK(ajm_batch_start(ctx,NULL,8,0,NULL,&batch)==ERR_INVALID_PARAMETER);
    CHECK(ajm_batch_start(ctx,jobs,1,0,NULL,&batch)==ERR_MALFORMED_BATCH);
    CHECK(ajm_batch_start(ctx,jobs,8,0,NULL,NULL)==ERR_INVALID_PARAMETER);
    Chunk malformed={header(IDENT_INLINE,0),16};memcpy(jobs,&malformed,8);
    CHECK(ajm_batch_start(ctx,jobs,8,0,NULL,&batch)==ERR_MALFORMED_BATCH);
    malformed=(Chunk){header(IDENT_JOB,instance),8};memcpy(jobs,&malformed,8);
    malformed=(Chunk){header(IDENT_INPUT_RUN,0),0};memcpy(jobs+8,&malformed,8);
    CHECK(ajm_batch_start(ctx,jobs,16,0,NULL,&batch)==ERR_MALFORMED_BATCH);
    CHECK(ajm_finalize(ctx)==0);CHECK(ajm_finalize(other)==0);CHECK(ajm_finalize(ctx)==ERR_INVALID_CONTEXT);
    CHECK(BitReverse32(0xffffffffu,0)==0);CHECK(BitReverse32(1,32)==0x80000000u);
    CHECK(SignExtend32(0xff,8)==-1);CHECK(SignExtend32(0x7f,8)==127);CHECK(SignExtend32(-1,32)==-1);
    printf("AJM contract: PASS checks=%u; actual MP3 decoded, ATRAC9 initialized, batch ABI/errors\n",checks);
}
