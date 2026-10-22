/* SPDX-License-Identifier: GPL-2.0-or-later
 * One opt-in diagnostic profile for the inspected big3 wait loop. The callback
 * observes the value already loaded by the guest; it does not read the object.
 */
#ifndef UNCHARTED_POLL_PROFILE_H
#define UNCHARTED_POLL_PROFILE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <cpuid.h>
#define UW_POLL_ENTRY UINT64_C(0x48c4c0)
#define UW_POLL_SITE UINT64_C(0x48c54e)
#define UW_POLL_COMPARE UINT64_C(0x49e1c0)
#define UW_POLL_THUNK 512
static const unsigned char uw_poll_entry[]={
    0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,
    0x41,0x89,0xd6,0x49,0x89,0xf7,0x48,0x89,0xfb};
static const unsigned char uw_poll_loop[]={
    0xbf,1,0,0,0,0xe8,0xc6,0xfa,0,0,0x48,0x8b,0x7b,0x20,
    0x4c,0x89,0xfe,0x44,0x89,0xf2,0xe8,0x67,0x1c,1,0,
    0x84,0xc0,0x74,0xe3,0x5b,0x41,0x5c,0x41,0x5e,0x41,0x5f,0x5d,0xc3};
static const unsigned char uw_poll_compare[]={
    0x83,0xfa,1,0x75,7,0x48,0x39,0xf7,0x0f,0x9e,0xc0,0xc3,
    0x85,0xd2,0x75,7,0x48,0x39,0xf7,0x0f,0x94,0xc0,0xc3,0x31,0xc0,0xc3};
static int uw_poll_match(const unsigned char *image,uint64_t size) {
    return image && size>=UW_POLL_COMPARE+sizeof(uw_poll_compare) &&
        !memcmp(image+UW_POLL_ENTRY,uw_poll_entry,sizeof(uw_poll_entry)) &&
        !memcmp(image+UW_POLL_SITE-14,uw_poll_loop,sizeof(uw_poll_loop)) &&
        !memcmp(image+UW_POLL_COMPARE,uw_poll_compare,sizeof(uw_poll_compare));
}
static int uw_poll_avx(void) {
    unsigned a,b,c,d,lo,hi;
    if(!__get_cpuid(1,&a,&b,&c,&d) || (c&0x18000000)!=0x18000000) return 0;
    __asm__ volatile("xgetbv" : "=a"(lo),"=d"(hi) : "c"(0));
    (void)hi;return (lo&6)==6;
}
static int uw_poll_rel32(uintptr_t from,uintptr_t to,int32_t *out) {
    if(to>=from) { if(to-from>INT32_MAX) return 0; *out=(int32_t)(to-from); }
    else { if(from-to>UINT64_C(2147483648)) return 0; *out=(int32_t)(-(int64_t)(from-to)); }
    return 1;
}
/* MS ABI callback(object, current, target, mode, original_wait_return).
 * Entry signature proves RBP is this routine's frame; [RBP+8] is its own
 * return slot. No frame walk or arbitrary object/data dereference is made.
 * Preserve all GPRs/flags and all 16 full YMM registers across the host call.
 */
static size_t uw_poll_emit(unsigned char *out,size_t capacity,uintptr_t address,
                           uintptr_t site,uintptr_t callback) {
    if(!out || capacity<UW_POLL_THUNK || !callback ||
       address>UINTPTR_MAX-UW_POLL_THUNK || site>UINTPTR_MAX-6) return 0;
    int32_t forward,back;
    if(site>UINTPTR_MAX-5 || !uw_poll_rel32(site+5,address,&forward)) return 0;
    unsigned char bytes[UW_POLL_THUNK];size_t k=0;
    static const unsigned char save[]={0x9c,0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53,0x53,
        0x48,0x89,0xe3,0x48,0x83,0xe4,0xf0,0x48,0x81,0xec,0x40,0x02,0,0};
    memcpy(bytes+k,save,sizeof(save));k+=sizeof(save);
    for(unsigned x=0;x<16;++x) {
        uint32_t disp=0x40+32*x;
        bytes[k++]=0xc5;bytes[k++]=x<8?0xfe:0x7e;bytes[k++]=0x7f;
        bytes[k++]=(unsigned char)(0x84|((x&7)<<3));bytes[k++]=0x24;
        memcpy(bytes+k,&disp,4);k+=4;
    }
    static const unsigned char args[]={
        0x48,0x8b,0x0b,       /* rcx = original rbx (object), saved on stack */
        0x48,0x89,0xfa,       /* rdx = rdi (current field) */
        0x4d,0x89,0xf8,       /* r8 = r15 (target) */
        0x4d,0x89,0xf1,       /* r9 = r14 (comparison mode) */
        0x48,0x8b,0x45,0x08, /* rax = [rbp+8] (wait routine return slot) */
        0x48,0x89,0x44,0x24,0x20}; /* fifth argument beyond shadow space */
    memcpy(bytes+k,args,sizeof(args));k+=sizeof(args);
    bytes[k++]=0x48;bytes[k++]=0xb8;memcpy(bytes+k,&callback,8);k+=8;
    bytes[k++]=0xff;bytes[k++]=0xd0;
    for(unsigned x=0;x<16;++x) {
        uint32_t disp=0x40+32*x;
        bytes[k++]=0xc5;bytes[k++]=x<8?0xfe:0x7e;bytes[k++]=0x6f;
        bytes[k++]=(unsigned char)(0x84|((x&7)<<3));bytes[k++]=0x24;
        memcpy(bytes+k,&disp,4);k+=4;
    }
    static const unsigned char restore[]={0x48,0x89,0xdc,0x5b,0x41,0x5b,0x41,0x5a,0x41,0x59,0x41,0x58,0x5a,0x59,0x58,0x9d};
    memcpy(bytes+k,restore,sizeof(restore));k+=sizeof(restore);
    memcpy(bytes+k,uw_poll_loop+14,6);k+=6; /* original position-independent moves */
    if(!uw_poll_rel32(address+k+5,site+6,&back)) return 0;
    bytes[k++]=0xe9;memcpy(bytes+k,&back,4);k+=4;
    if(k>sizeof(bytes)) return 0;
    memcpy(out,bytes,k);return k;
}
/* Site publication must happen only after this succeeds. Tests inject a seal
 * failure; the original guest instructions are never patched on that path. */
static size_t uw_poll_prepare(unsigned char *out,size_t capacity,uintptr_t address,
        uintptr_t site,uintptr_t callback,int (*seal)(void *,size_t)) {
    if(!seal) return 0;
    size_t bytes=uw_poll_emit(out,capacity,address,site,callback);
    return bytes && seal(out,capacity) ? bytes : 0;
}
#endif
