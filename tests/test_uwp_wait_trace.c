/* SPDX-License-Identifier: GPL-2.0-or-later
 * Real guest mutex/condition blocking with concurrent lock-free snapshots.
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define _GNU_SOURCE
#define UNCHARTED_WAIT_TRACE
#include "../src/runtime_wait_trace.c"
#include "../src/runtime_mutex.c"
#include <assert.h>
#include <sched.h>
static GuestMutex *test_mutex;
static GuestCond *test_cond;
static int test_release;
static void pause_ms(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec t={0,1000000};nanosleep(&t,NULL);
#endif
}
static void *blocked_worker(void *unused) {
    (void)unused;UW_NAME("blocked-worker");
    assert(mutex_lock(&test_mutex)==0);
    while(!test_release) assert(cond_wait(&test_cond,&test_mutex)==0);
    assert(mutex_unlock(&test_mutex)==0);
    UW_BEGIN(UW_EXITED,0);
    return NULL;
}
static void await_kind(unsigned index,unsigned kind) {
    for(unsigned i=0;i<5000;++i) {
        if(LOAD(uw_slots[index].kind)==kind&&!(LOAD(uw_slots[index].seq)&1)) return;
        pause_ms();
    }
    assert(!"worker did not reach expected wait");
}
static void snapshot_contains(const char *needle) {
    FILE *f=tmpfile();assert(f);uncharted_wait_snapshot(f);rewind(f);
    char buffer[16384];size_t n=fread(buffer,1,sizeof(buffer)-1,f);buffer[n]=0;
    fclose(f);assert(strstr(buffer,needle));
}
static void *overflow_worker(void *unused) {
    (void)unused;UW_NAME("capacity-worker");
    UW_BEGIN(UW_SLEEP,1);UW_END(-60);return NULL;
}
int main(void) {
    UW_BEGIN(UW_MUTEX,1);UW_END(0);assert(LOAD(uw_claimed)==0);
    uncharted_wait_enable();UW_NAME("monitor");
    errno=EINTR;UW_BEGIN(UW_MUTEX,0x1234);assert(errno==EINTR);
    UW_END(-60);assert(errno==EINTR);assert(LOAD(uw_slots[0].result)==(uint64_t)-60);
    assert(mutex_init(&test_mutex,NULL,"test")==0);
    assert(cond_init(&test_cond,NULL,"test")==0);
    assert(mutex_lock(&test_mutex)==0);
    pthread_t worker;assert(pthread_create(&worker,NULL,blocked_worker,NULL)==0);
    await_kind(1,UW_MUTEX);snapshot_contains("name=blocked-worker state=mutex");
    assert(LOAD(uw_slots[1].caller)!=0);
    assert(mutex_unlock(&test_mutex)==0);
    await_kind(1,UW_CONDITION);snapshot_contains("name=blocked-worker state=condition");
    for(unsigned i=0;i<100;++i) snapshot_contains("UWP guest tasks:");
    assert(mutex_lock(&test_mutex)==0);test_release=1;
    assert(cond_signal(&test_cond)==0);assert(mutex_unlock(&test_mutex)==0);
    assert(pthread_join(worker,NULL)==0);
    snapshot_contains("name=blocked-worker state=exited");
    assert(LOAD(uw_slots[1].completed)>=2);
    assert(cond_destroy(&test_cond)==0);assert(mutex_destroy(&test_mutex)==0);
    /* Slots never alias after thread exit or capacity exhaustion. */
    for(unsigned i=0;i<UW_SLOTS+2;++i) {
        assert(pthread_create(&worker,NULL,overflow_worker,NULL)==0);
        assert(pthread_join(worker,NULL)==0);
    }
    assert(LOAD(uw_claimed)==UW_SLOTS+4);
    snapshot_contains("capacity=256 dropped=4");
    puts("Guest wait trace contract: PASS; real mutex/condition waits, wakeup, errno/results, snapshots, thread exit and capacity");
    return 0;
}
