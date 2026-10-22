/* SPDX-License-Identifier: GPL-2.0-or-later
 * Generated code: allocate writable, fill it, then seal read/execute.
 * The Windows contract uses the same FromApp APIs as the packaged UWP host.
 */
#ifndef UNCHARTED_CODE_MEMORY_H
#define UNCHARTED_CODE_MEMORY_H
#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
static void *uw_code_allocate_at(void *address,size_t size) {
    return VirtualAllocFromApp(address,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
}
static int uw_code_seal(void *address,size_t size) {
    DWORD previous;
    if(!VirtualProtectFromApp(address,size,PAGE_EXECUTE_READ,&previous)) return 0;
    return FlushInstructionCache(GetCurrentProcess(),address,size)!=0;
}
static int uw_code_release(void *address,size_t size) {
    (void)size;return VirtualFree(address,0,MEM_RELEASE)!=0;
}
#else
#include <sys/mman.h>
static void *uw_code_allocate_at(void *address,size_t size) {
    void *p=mmap(address,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==MAP_FAILED) return NULL;
    if(address && p!=address) { munmap(p,size);return NULL; }
    return p;
}
static int uw_code_seal(void *address,size_t size) {
    if(mprotect(address,size,PROT_READ|PROT_EXEC)) return 0;
    __builtin___clear_cache((char *)address,(char *)address+size);return 1;
}
static int uw_code_release(void *address,size_t size) { return munmap(address,size)==0; }
#endif
#endif
