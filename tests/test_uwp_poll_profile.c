/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define _GNU_SOURCE
#include "../src/runtime_poll_profile.h"
#include "../src/runtime_wait_trace.c"
#include "../src/runtime_code_memory.h"
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
typedef struct { uint64_t regs[16];unsigned char ymm[512]; } PollOutput;
const unsigned char poll_pattern[32]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
    17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32};
uintptr_t poll_expected_caller;
extern __attribute__((sysv_abi)) void poll_contract_call(void *site,PollOutput *out,
    uint64_t object,uint64_t current,uint64_t target,uint64_t mode);
#define LOAD_YMM(n) "vmovdqu poll_pattern(%rip),%ymm" #n "\n"
#define SAVE_YMM(n,offset) "vmovdqu %ymm" #n "," #offset "(%r13)\n"
__asm__(".text\n.globl poll_contract_call\npoll_contract_call:\n"
    "push %rbp\nmov %rsp,%rbp\npush %rbx\npush %r12\npush %r13\npush %r14\npush %r15\nsub $8,%rsp\n"
    "mov 8(%rbp),%rax\nmov %rax,poll_expected_caller(%rip)\n"
    "mov %rdi,%r12\nmov %rsi,%r13\nmov %rdx,%rbx\nmov %rcx,%rdi\nmov %r8,%r15\nmov %r9,%r14\n"
    LOAD_YMM(0) LOAD_YMM(1) LOAD_YMM(2) LOAD_YMM(3) LOAD_YMM(4) LOAD_YMM(5) LOAD_YMM(6) LOAD_YMM(7)
    LOAD_YMM(8) LOAD_YMM(9) LOAD_YMM(10) LOAD_YMM(11) LOAD_YMM(12) LOAD_YMM(13) LOAD_YMM(14) LOAD_YMM(15)
    "mov $0x1111,%rax\nmov $0x2222,%rcx\nmov $0x3333,%rdx\nmov $0x4444,%r8\nmov $0x5555,%r9\n"
    "mov $0x6666,%r10\nmov $0x7777,%r11\nstc\ncall *%r12\n"
    "mov %rax,0(%r13)\nmov %rcx,8(%r13)\nmov %rdx,16(%r13)\nmov %rbx,24(%r13)\n"
    "mov %rsi,32(%r13)\nmov %rdi,40(%r13)\nmov %r8,48(%r13)\nmov %r9,56(%r13)\n"
    "mov %r10,64(%r13)\nmov %r11,72(%r13)\nmov %r12,80(%r13)\nmov %r13,88(%r13)\n"
    "mov %r14,96(%r13)\nmov %r15,104(%r13)\npushfq\npop %rax\nmov %rax,120(%r13)\n"
    SAVE_YMM(0,128) SAVE_YMM(1,160) SAVE_YMM(2,192) SAVE_YMM(3,224)
    SAVE_YMM(4,256) SAVE_YMM(5,288) SAVE_YMM(6,320) SAVE_YMM(7,352)
    SAVE_YMM(8,384) SAVE_YMM(9,416) SAVE_YMM(10,448) SAVE_YMM(11,480)
    SAVE_YMM(12,512) SAVE_YMM(13,544) SAVE_YMM(14,576) SAVE_YMM(15,608)
    "vzeroupper\nadd $8,%rsp\npop %r15\npop %r14\npop %r13\npop %r12\npop %rbx\npop %rbp\nret\n");
static void __attribute__((ms_abi,noinline)) capture(uint64_t object,uint64_t current,
        uint64_t target,uint64_t mode,uint64_t caller,uint64_t site) {
    uncharted_wait_poll(object,current,target,mode,caller,site);
    /* Simulate a host function that destroys volatile registers and YMM upper
     * halves. The compiler still obeys the MS ABI for XMM6..15 lower halves. */
    __asm__ volatile("vzeroall" ::: "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
        "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15");
}
static void *worker(void *arg) {
    uintptr_t id=(uintptr_t)arg;
    uncharted_wait_name(id==1?"poll-worker-1":"poll-worker-2");
    for(unsigned i=0;i<10000;++i) uncharted_wait_poll(id,id+1,id+2,id+3,id+4,id+5);
    return NULL;
}
static unsigned seal_failures;
static int reject_seal(void *address,size_t size) {
    (void)address;(void)size;++seal_failures;return 0;
}
static int reject_second_seal(void *address,size_t size) {
    if(++seal_failures==2) return 0;
    return uw_code_seal(address,size);
}
int main(void) {
    assert(uw_poll_avx());
    const size_t size=UW_POLL_COMPARE+sizeof(uw_poll_compare);
    unsigned char *fixture=calloc(1,size);assert(fixture);
    memcpy(fixture+UW_POLL_ENTRY,uw_poll_entry,sizeof(uw_poll_entry));
    memcpy(fixture+UW_POLL_SITE-14,uw_poll_loop,sizeof(uw_poll_loop));
    memcpy(fixture+UW_POLL_WORKER_SITE-18,uw_poll_worker,sizeof(uw_poll_worker));
    memcpy(fixture+UW_POLL_COMPARE,uw_poll_compare,sizeof(uw_poll_compare));
    assert(uw_poll_match(fixture,size));assert(!uw_poll_match(fixture,size-1));assert(!uw_poll_match(NULL,size));
    const size_t offsets[]={UW_POLL_ENTRY,UW_POLL_SITE-14,UW_POLL_WORKER_SITE-18,UW_POLL_COMPARE};
    const size_t lengths[]={sizeof(uw_poll_entry),sizeof(uw_poll_loop),sizeof(uw_poll_worker),sizeof(uw_poll_compare)};
    for(unsigned i=0;i<4;++i) for(size_t j=0;j<lengths[i];++j) {
        fixture[offsets[i]+j]^=1;assert(!uw_poll_match(fixture,size));fixture[offsets[i]+j]^=1;
    }
    free(fixture);
#ifdef _WIN32
    /* This rejected UWP allocation is precisely what the old desktop-only
     * VirtualAlloc contract failed to exercise in 0.6.11. */
    void *forbidden=VirtualAllocFromApp(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    if(forbidden) { VirtualFree(forbidden,0,MEM_RELEASE);assert(!"FromApp unexpectedly accepted RWX"); }
#endif
    /* Separate pages: sealing the thunk must not seal the still-writable
     * synthetic guest site before its branch is published. */
    unsigned char *failure=uw_code_allocate_at(NULL,12288);assert(failure);
    uintptr_t failure_image=(uintptr_t)(failure+8192)-UW_POLL_WORKER_SITE;
    for(unsigned i=0;i<2;++i) memcpy((void *)(failure_image+uw_poll_sites[i]),uw_poll_loop+14,6);
    int32_t jumps[2];seal_failures=0;
    assert(!uw_poll_prepare_all(failure,4096,failure_image,(uintptr_t)capture,jumps,reject_second_seal));
    assert(seal_failures==2);
    for(unsigned i=0;i<2;++i) assert(!memcmp((void *)(failure_image+uw_poll_sites[i]),uw_poll_loop+14,6));
    assert(uw_code_release(failure,12288));
    unsigned char *memory=uw_code_allocate_at(NULL,12288);assert(memory);
    uintptr_t image=(uintptr_t)(memory+8192)-UW_POLL_WORKER_SITE;
    for(unsigned i=0;i<2;++i) {
        unsigned char *site=(void *)(image+uw_poll_sites[i]);
        memcpy(site,uw_poll_loop+14,6);site[6]=0xc3;
    }
    memset(memory,0x5a,UW_POLL_THUNK);
    assert(!uw_poll_emit(memory,511,(uintptr_t)memory,image+UW_POLL_SITE,(uintptr_t)capture));
    assert(!uw_poll_emit(memory,512,UINTPTR_MAX-1,image+UW_POLL_SITE,(uintptr_t)capture));
    assert(!uw_poll_emit(memory,512,(uintptr_t)memory,1,(uintptr_t)capture));
    assert(memory[0]==0x5a);
    assert(!uw_poll_prepare(memory,4096,(uintptr_t)memory,image+UW_POLL_SITE,(uintptr_t)capture,reject_seal));
    assert(uw_poll_prepare_all(memory,4096,image,(uintptr_t)capture,jumps,uw_code_seal));
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info;
    for(unsigned i=0;i<2;++i) {
        assert(VirtualQuery(memory+i*4096,&info,sizeof(info)));assert(info.Protect==PAGE_EXECUTE_READ);
    }
    assert(VirtualQuery(memory+8192,&info,sizeof(info)));assert(info.Protect==PAGE_READWRITE);
#endif
    for(unsigned i=0;i<2;++i) {
        unsigned char *site=(void *)(image+uw_poll_sites[i]);
        site[0]=0xe9;memcpy(site+1,&jumps[i],4);site[5]=0x90;
    }
    assert(uw_code_seal(memory+8192,4096));
    PollOutput out={0};
    for(unsigned i=0;i<2;++i) poll_contract_call((void *)(image+uw_poll_sites[i]),&out,1,17,9,1);
    assert(LOAD(uw_claimed)==0);
    uncharted_wait_enable();uncharted_wait_name("poll-contract");
    for(unsigned i=0;i<2;++i) {
        unsigned char *site=(void *)(image+uw_poll_sites[i]);
        errno=EDOM;poll_contract_call(site,&out,1,17,9,1);assert(errno==EDOM);
        UwSlot *s=&uw_slots[0];assert(LOAD(s->poll_object)==1&&LOAD(s->poll_current)==17);
        assert(LOAD(s->poll_target)==9&&LOAD(s->poll_mode)==1);
        assert(LOAD(s->poll_caller)==poll_expected_caller&&LOAD(s->poll_samples)==i+1);
        assert(LOAD(s->poll_site)==(uintptr_t)site);
        assert(out.regs[0]==0x1111&&out.regs[1]==0x2222&&out.regs[2]==1&&out.regs[3]==1);
        assert(out.regs[4]==9&&out.regs[5]==17&&out.regs[6]==0x4444&&out.regs[7]==0x5555);
        assert(out.regs[8]==0x6666&&out.regs[9]==0x7777&&out.regs[10]==(uintptr_t)site);
        assert(out.regs[11]==(uintptr_t)&out&&out.regs[12]==1&&out.regs[13]==9&&(out.regs[15]&1));
        for(unsigned i=0;i<16;++i) assert(!memcmp(out.ymm+32*i,poll_pattern,32));
    }
    pthread_t a,b;assert(!pthread_create(&a,NULL,worker,(void*)1));assert(!pthread_create(&b,NULL,worker,(void*)2));
    assert(!pthread_join(a,NULL));assert(!pthread_join(b,NULL));
    assert(LOAD(uw_claimed)==3);
    for(unsigned i=1;i<3;++i) {
        UwSlot *w=&uw_slots[i];uint64_t id=LOAD(w->poll_object);
        assert(id==1||id==2);assert(LOAD(w->poll_current)==id+1&&LOAD(w->poll_target)==id+2);
        assert(LOAD(w->poll_mode)==id+3&&LOAD(w->poll_caller)==id+4&&LOAD(w->poll_samples)==10000);
        assert(LOAD(w->poll_site)==id+5);
    }
    assert(LOAD(uw_slots[1].poll_object)!=LOAD(uw_slots[2].poll_object));
    FILE *f=tmpfile();assert(f);uncharted_wait_snapshot(f);rewind(f);
    char text[4096];size_t n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);
    assert(strstr(text,"poll_last: object=0x1 current=0x11 target=0x9 mode=1"));
    assert(strstr(text,"sample_age_ms="));
    assert(uw_code_release(memory,12288));
    puts("Loading poll profile contract: PASS; pause and cooperative sites, six MS-ABI arguments, RW-to-RX shared allocator/sealer, failed second seal before any publication, signature rejection, real emitted thunk, MS ABI, GPR/flags/all YMM preservation, no object reads and per-thread samples");
    return 0;
}
