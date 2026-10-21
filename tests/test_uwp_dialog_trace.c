#include "xbox/platform/dialog_trace.h"
#include <string.h>
#include <stdlib.h>
static unsigned checks,reads;
#define CHECK(x) do { ++checks;if(!(x)){fprintf(stderr,"Dialog trace FAIL %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static UnchartedDialogPrefix param={48,{0},136,1,0,0x2000,0x2000,0x4000};
static struct {uint32_t type,pad;uint64_t text;} user={5,0,0x3000};
static char text[1200]="Game message\nLine 2";
static uint32_t system_type=4;
static int read_guest(void* context,const void* address,void* out,size_t size) {
    (void)context;++reads;uintptr_t a=(uintptr_t)address;
    const void* p=NULL;size_t n=0;uintptr_t base=0;
    if(a>=0x1000&&a<0x1000+sizeof(param)){p=&param;n=sizeof(param);base=0x1000;}
    else if(a>=0x2000&&a<0x2000+sizeof(user)){p=&user;n=sizeof(user);base=0x2000;}
    else if(a>=0x3000&&a<0x3000+sizeof(text)){p=text;n=sizeof(text);base=0x3000;}
    else if(a==0x4000){p=&system_type;n=sizeof(system_type);base=0x4000;}
    if(!p||size>n-(a-base)) return 0;
    memcpy(out,(const unsigned char*)p+(a-base),size);return 1;
}
static int run(char* output,size_t size) {
    FILE* f=tmpfile();CHECK(f!=NULL);int r=uncharted_dialog_trace(f,(void*)0x1000,read_guest,NULL);
    rewind(f);size_t n=fread(output,1,size-1,f);output[n]=0;fclose(f);return r;
}
int main(void){
    char output[4096];CHECK(run(output,sizeof(output))==1);CHECK(strstr(output,"Game message Line 2")!=NULL);
    param.mode=2;CHECK(run(output,sizeof(output))==1);CHECK(strstr(output,"progress: type=5")!=NULL);
    param.mode=3;CHECK(run(output,sizeof(output))==1);CHECK(strstr(output,"system message: type=4")!=NULL);
    param.mode=1;user.text=UINT64_MAX;CHECK(run(output,sizeof(output))==0);CHECK(strstr(output,"unreadable message string")!=NULL);
    user.text=0x3000;memset(text,'X',sizeof(text));CHECK(run(output,sizeof(output))==1);CHECK(strstr(output,"[truncated]")!=NULL);
    param.size=120;reads=0;CHECK(run(output,sizeof(output))==0);CHECK(reads==1);
    param.size=136;param.mode=99;CHECK(run(output,sizeof(output))==0);CHECK(strstr(output,"unknown mode")!=NULL);
    printf("Dialog trace contract: PASS checks=%u; layouts, modes, bounded text and unreadable pointers\n",checks);
}
