/* SPDX-License-Identifier: GPL-2.0-or-later
 * Exercise the actual exported sleep entries from an exact SysV call site.
 * The synthetic guest caller has no frame pointer or Windows unwind records.
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define _GNU_SOURCE
#define UNCHARTED_WAIT_TRACE
#include "../src/runtime_wait_trace.c"
#include "../src/runtime_kernel.c"
#include <assert.h>
static int32_t guest_errno;
int32_t *runtime_errno(void) { return &guest_errno; }
void runtime_report(void) { assert(!"unexpected process termination"); }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t count,const char *name) {
    for(size_t i=0;i<count;++i)
        if(!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}
extern ABI int32_t sleep_guest_call(uintptr_t function,uintptr_t a,uintptr_t b);
extern const char sleep_guest_return[];
__asm__(".text\n"
        ".globl sleep_guest_call\n"
        "sleep_guest_call:\n"
        "mov %rdi,%rax\nmov %rsi,%rdi\nmov %rdx,%rsi\n"
        "sub $8,%rsp\ncall *%rax\n"
        ".globl sleep_guest_return\n"
        "sleep_guest_return:\nadd $8,%rsp\nret\n");
static void check_sleep(const char *name,uintptr_t a,uintptr_t b,uint64_t ns) {
    uintptr_t function=runtime_kernel_resolve(name);assert(function);
    uint64_t before=LOAD(uw_slots[0].calls);
    guest_errno=77;errno=EDOM;
    assert(sleep_guest_call(function,a,b)==0);
    assert(guest_errno==77);assert(errno==EDOM);
    assert(LOAD(uw_slots[0].caller)==(uintptr_t)sleep_guest_return);
    assert(LOAD(uw_slots[0].target)==ns);
    assert(LOAD(uw_slots[0].kind)==UW_GUEST);
    assert(LOAD(uw_slots[0].calls)==before+1);
    assert(LOAD(uw_slots[0].completed)==before+1);
    assert(LOAD(uw_slots[0].result)==0);
}
int main(void) {
    /* Tracing is disabled until explicitly enabled, including real sleep. */
    assert(sleep_guest_call(runtime_kernel_resolve("sceKernelUsleep"),1,0)==0);
    assert(LOAD(uw_claimed)==0);
    uncharted_wait_enable();UW_NAME("sleep-contract");
    check_sleep("sceKernelUsleep",1000,0,1000000);
    check_sleep("usleep",500,0,500000);
    check_sleep("sleep",1,0,1000000000);
    GuestTimespec request={0,250000},remaining={17,29};
    check_sleep("nanosleep",(uintptr_t)&request,(uintptr_t)&remaining,250000);
    assert(remaining.sec==0&&remaining.nsec==0);
    remaining=(GuestTimespec){17,29};
    check_sleep("sceKernelNanosleep",(uintptr_t)&request,(uintptr_t)&remaining,250000);
    assert(remaining.sec==0&&remaining.nsec==0);
    uint64_t calls=LOAD(uw_slots[0].calls);
    const char *names[]={"nanosleep","sceKernelNanosleep"};
    for(unsigned i=0;i<2;++i) {
        uintptr_t function=runtime_kernel_resolve(names[i]);
        int32_t expected=i?(int32_t)UINT32_C(0x80020016):-1;
        assert(sleep_guest_call(function,0,0)==expected);assert(guest_errno==22);
        request=(GuestTimespec){0,1000000000};remaining=(GuestTimespec){17,29};
        assert(sleep_guest_call(function,(uintptr_t)&request,(uintptr_t)&remaining)==expected);
        assert(guest_errno==22&&remaining.sec==17&&remaining.nsec==29);
        request=(GuestTimespec){-1,0};
        assert(sleep_guest_call(function,(uintptr_t)&request,0)==expected);
        request=(GuestTimespec){0,-1};
        assert(sleep_guest_call(function,(uintptr_t)&request,0)==expected);
    }
    assert(LOAD(uw_slots[0].calls)==calls);
    FILE *f=tmpfile();assert(f);uncharted_wait_snapshot(f);rewind(f);
    char line[2048];size_t n=fread(line,1,sizeof(line)-1,f);line[n]=0;fclose(f);
    char expected[80];snprintf(expected,sizeof(expected),"call_site=0x%llx",(unsigned long long)(uintptr_t)sleep_guest_return);
    assert(strstr(line,expected));
    puts("Sleep trace contract: PASS; exact guest return address at all five exports, disabled path, units, errno, results and invalid requests");
    return 0;
}
