/* SPDX-License-Identifier: MIT */
#include "xbox/graphics/dzn_diagnostic_sink.h"
#include "xbox/graphics/dzn_map_guard.h"
#include <stdlib.h>
#include <string.h>
static unsigned checks,calls;
static char last[2048];
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Map guard FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void collect(const char *s){++calls;snprintf(last,sizeof(last),"%s",s);}
int main(void){
    unsigned marker=0x12345678;
    dznSetUwpDiagnosticCallback(collect);
    CHECK(dznUwpDiagnosticMapFailure()==0);
    CHECK(dzn_uwp_check_map(0,&marker,0)==0);CHECK(calls==0);
    CHECK(dzn_uwp_check_map(1,&marker,0)==0);CHECK(calls==0);
    CHECK(dzn_uwp_check_map((int32_t)0x80004005,NULL,0)==-13);CHECK(calls==2);
    CHECK(dzn_uwp_check_map(0,NULL,0)==-13);CHECK(calls==4);
    CHECK(dzn_uwp_check_map((int32_t)0x8007000e,&marker,0)==-1);CHECK(calls==6);
    CHECK(dzn_uwp_check_map((int32_t)0x887a0005,NULL,(int32_t)0x887a0006)==-13);CHECK(calls==8);
    CHECK(strstr(last,"command_device_removed_reason")&&strstr(last,"0x887a0006"));
    CHECK(marker==0x12345678);
    dznSetUwpDiagnosticMapFailure((int32_t)0x80004005);CHECK(dznUwpDiagnosticMapFailure()<0);
    dznSetUwpDiagnosticMapFailure(0);CHECK(dznUwpDiagnosticMapFailure()==0);
    printf("Dozen map guard contract: PASS checks=%u; failed/null maps, lost device, success and injection reset\n",checks);
}
