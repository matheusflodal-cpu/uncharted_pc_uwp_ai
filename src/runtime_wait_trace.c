/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bounded, best-effort snapshots of host threads running guest tasks. No
 * monitor locks, guest dereferences, stack walking or forced thread suspension.
 * These are API call sites, not sampled instruction pointers or fiber states.
 */
#define _GNU_SOURCE
#include "runtime_wait_trace.h"
#include <errno.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define UW_SLOTS 256
#define LOAD(x) __atomic_load_n(&(x),__ATOMIC_SEQ_CST)
#define STORE(x,v) __atomic_store_n(&(x),(v),__ATOMIC_SEQ_CST)
typedef struct {
    uint64_t seq,start,calls,completed,target,caller,result;
    uint64_t poll_samples,poll_time,poll_object,poll_current,poll_target,poll_mode,poll_caller,poll_site;
    unsigned kind;
    unsigned char name[32];
} UwSlot;
static UwSlot uw_slots[UW_SLOTS];
static unsigned uw_enabled,uw_claimed;
static _Thread_local unsigned uw_local;
static uint64_t uw_now(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
#endif
}
static UwSlot *uw_current(void) {
    if(!LOAD(uw_enabled)) return NULL;
    if(!uw_local) {
        unsigned id=__atomic_fetch_add(&uw_claimed,1,__ATOMIC_SEQ_CST);
        uw_local=id<UW_SLOTS?id+1:UINT32_MAX;
    }
    return uw_local<=UW_SLOTS ? &uw_slots[uw_local-1] : NULL;
}
static void uw_write(UwSlot *s) { __atomic_add_fetch(&s->seq,1,__ATOMIC_SEQ_CST); }
void uncharted_wait_enable(void) { STORE(uw_enabled,1); }
void uncharted_wait_name(const char *name) {
    int saved=errno;UwSlot *s=uw_current();
    if(s) {
        uw_write(s);unsigned i=0;
        if(name) for(;i<31&&name[i];++i) STORE(s->name[i],(unsigned char)name[i]);
        for(;i<32;++i) STORE(s->name[i],0);
        uw_write(s);
    }
    errno=saved;
}
void uncharted_wait_begin(unsigned kind,uintptr_t target,uintptr_t caller) {
    int saved=errno;UwSlot *s=uw_current();
    if(s) {
        uw_write(s);STORE(s->kind,kind);STORE(s->target,(uint64_t)target);
        STORE(s->caller,(uint64_t)caller);STORE(s->start,uw_now());
        STORE(s->calls,LOAD(s->calls)+1);uw_write(s);
    }
    errno=saved;
}
void uncharted_wait_end(int64_t result) {
    int saved=errno;UwSlot *s=uw_current();
    if(s) {
        uw_write(s);STORE(s->kind,UW_GUEST);STORE(s->result,(uint64_t)result);
        STORE(s->completed,LOAD(s->completed)+1);uw_write(s);
    }
    errno=saved;
}
void __attribute__((ms_abi)) uncharted_wait_poll(uint64_t object,uint64_t current,
        uint64_t target,uint64_t mode,uint64_t caller,uint64_t site) {
    int saved=errno;UwSlot *s=uw_current();
    if(s) {
        uw_write(s);STORE(s->poll_object,object);STORE(s->poll_current,current);
        STORE(s->poll_target,target);STORE(s->poll_mode,mode);STORE(s->poll_caller,caller);
        STORE(s->poll_site,site);
        STORE(s->poll_time,uw_now());STORE(s->poll_samples,LOAD(s->poll_samples)+1);uw_write(s);
    }
    errno=saved;
}
void uncharted_wait_snapshot(FILE *out) {
    if(!LOAD(uw_enabled)) return;
    static const char *const kinds[]={"guest_or_untraced","mutex","condition","semaphore","join","sleep","equeue","rwlock","file","exited"};
    const unsigned claimed=LOAD(uw_claimed),count=claimed<UW_SLOTS?claimed:UW_SLOTS;
    const uint64_t now=uw_now();unsigned unstable=0;
    fprintf(out,"UWP guest tasks: registered=%u capacity=%u dropped=%u\n",count,UW_SLOTS,claimed>UW_SLOTS?claimed-UW_SLOTS:0);
    for(unsigned i=0;i<count;++i) {
        UwSlot *s=&uw_slots[i];uint64_t seq=LOAD(s->seq);if(!seq||(seq&1)) {++unstable;continue;}
        char name[32];for(unsigned j=0;j<32;++j) name[j]=(char)LOAD(s->name[j]);name[31]=0;
        unsigned kind=LOAD(s->kind);uint64_t start=LOAD(s->start),target=LOAD(s->target),caller=LOAD(s->caller);
        uint64_t calls=LOAD(s->calls),completed=LOAD(s->completed),result=LOAD(s->result);
        uint64_t samples=LOAD(s->poll_samples),poll_time=LOAD(s->poll_time),object=LOAD(s->poll_object);
        uint64_t current=LOAD(s->poll_current),wanted=LOAD(s->poll_target),mode=LOAD(s->poll_mode),poll_caller=LOAD(s->poll_caller);
        uint64_t poll_site=LOAD(s->poll_site);
        if(seq!=LOAD(s->seq)) {++unstable;continue;}
        fprintf(out,"  guest_task=%u name=%s state=%s target=0x%llx call_site=0x%llx age_ms=%llu calls=%llu returned=%llu last_result=0x%llx\n",
            i,name[0]?name:"unnamed",kind<sizeof(kinds)/sizeof(*kinds)?kinds[kind]:"unknown",
            (unsigned long long)target,(unsigned long long)caller,
            (unsigned long long)(kind&&start&&now>=start?now-start:0),
            (unsigned long long)calls,(unsigned long long)completed,(unsigned long long)result);
        if(samples) fprintf(out,"    poll_last: object=0x%llx current=0x%llx target=0x%llx mode=%llu caller=0x%llx samples=%llu sample_age_ms=%llu site=0x%llx\n",
            (unsigned long long)object,(unsigned long long)current,(unsigned long long)wanted,
            (unsigned long long)mode,(unsigned long long)poll_caller,(unsigned long long)samples,
            (unsigned long long)(now>=poll_time?now-poll_time:0),(unsigned long long)poll_site);
    }
    if(unstable) fprintf(out,"  guest_tasks_changed_during_snapshot=%u\n",unstable);
}
