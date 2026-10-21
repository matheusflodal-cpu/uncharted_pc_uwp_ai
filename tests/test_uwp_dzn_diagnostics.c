/* SPDX-License-Identifier: MIT */
#include "xbox/graphics/dzn_diagnostic_sink.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
static unsigned checks,calls;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static char last[2048];
#define CHECK(x) do { ++checks;if(!(x)) {fprintf(stderr,"Dozen sink FAIL %d: %s\n",__LINE__,#x);exit(1);} }while(0)
static void collect(const char *s) {
    pthread_mutex_lock(&lock);
    if(strlen(s)>=sizeof(last)) abort();
    strcpy(last,s);++calls;
    pthread_mutex_unlock(&lock);
}
static void *worker(void *p) {
    (void)p;for(unsigned i=0;i<100;++i)dzn_uwp_report("dxil_validation",5,"parallel message");return NULL;
}
int main(void) {
    dzn_uwp_report("disabled",1,"ignored");CHECK(calls==0);
    dznSetUwpDiagnosticCallback(collect);
    dzn_uwp_report("d3d12_graphics_pipeline",UINT32_C(0x80070057),"CreatePipelineState failed");
    CHECK(calls==1);CHECK(strstr(last,"phase=d3d12_graphics_pipeline")!=NULL);
    CHECK(strstr(last,"detail=0x80070057")!=NULL);CHECK(strstr(last,"CreatePipelineState failed")!=NULL);
    dzn_uwp_report(NULL,0,NULL);CHECK(strstr(last,"phase=unknown")&&strstr(last,"detail=0x00000000"));
    char huge[10000];memset(huge,'x',sizeof(huge));huge[sizeof(huge)-1]=0;
    dzn_uwp_report(huge,42,huge);CHECK(strlen(last)<sizeof(last));CHECK(strlen(last)>1600);
    pthread_t threads[8];for(unsigned i=0;i<8;++i)CHECK(pthread_create(&threads[i],NULL,worker,NULL)==0);
    for(unsigned i=0;i<8;++i)CHECK(pthread_join(threads[i],NULL)==0);
    CHECK(calls==803);dznSetUwpDiagnosticCallback(NULL);dzn_uwp_report("disabled",0,NULL);CHECK(calls==803);
    printf("Dozen diagnostic sink contract: PASS checks=%u; native status, bounded text, disabled sink and concurrency\n",checks);
}
