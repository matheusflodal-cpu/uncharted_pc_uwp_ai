/* SPDX-License-Identifier: GPL-2.0-or-later
 * Anonymous libkernel mappings use the real guest memory backend on each host.
 */
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PAGE UINT64_C(16384)
#ifdef UNCHARTED_STANDALONE_MEMORY_CONTRACT
static int32_t guest_errno;
int32_t *runtime_errno(void) { return &guest_errno; }
uintptr_t runtime_lookup(const RuntimeExport* entries,size_t count,const char* nid) {
    const char* name=!strcmp(nid,"BPE9s9vQQXo#p#J") ? "mmap" : !strcmp(nid,"tZY4+SZNFhA#p#J") ? "msync" : !strcmp(nid,"UqDGjXA5yUM#p#J") ? "munmap" : "";
    for(size_t i=0;i<count;++i) if(!strcmp(entries[i].name,name)) return (uintptr_t)entries[i].function;
    return 0;
}
#endif
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
int uncharted_posix_memory_contract(void) {
    runtime_memory_reserve();
    typedef void *(ABI *Map)(void*,uint64_t,int,int,int,int64_t);
    typedef int32_t (ABI *Sync)(void*,uint64_t,int);
    Map map=(Map)runtime_memory_resolve("BPE9s9vQQXo#p#J");
    Sync sync=(Sync)runtime_memory_resolve("tZY4+SZNFhA#p#J");
    typedef int32_t (ABI *Unmap)(void*,uint64_t);
    Unmap unmap=(Unmap)runtime_memory_resolve("UqDGjXA5yUM#p#J");
    CHECK(map&&sync&&unmap);
    void *failed=(void *)(uintptr_t)UINTPTR_MAX;
    CHECK(map(NULL,0,3,0x1002,-1,0)==failed&&*runtime_errno()==22);
    CHECK(map(NULL,1,3,2,3,0)==failed&&*runtime_errno()==78);
    CHECK(map(NULL,1,3,0x1003,-1,0)==failed&&*runtime_errno()==22);
    CHECK(map(NULL,1,3,0x1002,-1,1)==failed&&*runtime_errno()==22);
    CHECK(map((void*)1,1,3,0x1012,-1,0)==failed&&*runtime_errno()==22);
    CHECK(map(NULL,UINT64_MAX,3,0x1002,-1,0)==failed&&*runtime_errno()==22);
    unsigned char* bytes=map(NULL,PAGE+3,3,0x1002,-1,0);
    CHECK(bytes!=failed&&((uintptr_t)bytes%PAGE)==0);
    for(size_t i=0;i<PAGE*2;++i) CHECK(bytes[i]==0);
    bytes[0]=0x73; bytes[PAGE*2-1]=0x9a;
    CHECK(sync(bytes,PAGE*2,4)==0&&bytes[0]==0x73&&bytes[PAGE*2-1]==0x9a);
    CHECK(sync(bytes+1,PAGE,4)==-1&&*runtime_errno()==22);
    CHECK(sync(bytes,PAGE,5)==-1&&*runtime_errno()==22);
    CHECK(unmap(bytes,PAGE+1)==0);
    CHECK(!runtime_memory_is_mapped((uintptr_t)bytes,PAGE));
    CHECK(sync(bytes,PAGE,4)==-1&&*runtime_errno()==12);
    CHECK(unmap((void*)1,PAGE)==-1&&*runtime_errno()==22);
    bytes=map(NULL,PAGE,3,0x1001,-1,0);
    CHECK(bytes!=failed); CHECK(bytes[0]==0); CHECK(unmap(bytes,PAGE)==0);
    printf("POSIX memory contract: PASS checks=%u; anonymous mmap, zero-fill, msync, munmap and errors\n",checks);
    return 0;
}

#ifdef UNCHARTED_STANDALONE_MEMORY_CONTRACT
int main(void) { return uncharted_posix_memory_contract(); }
#endif
