/* SPDX-License-Identifier: GPL-2.0-or-later
 * Desktop Windows harness for the same FromApp memory backend linked into the
 * UWP application. No game files or renderer. The 5 GiB budget is a policy input,
 * not an OS-enforced AppContainer limit; Xbox validation is still necessary.
 */
#include <windows.h>
#include <psapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int uncharted_probe_main(int argc,char **argv);
uint64_t uncharted_uwp_app_memory_usage(void) {
    PROCESS_MEMORY_COUNTERS_EX info;
    memset(&info,0,sizeof(info)); info.cb=sizeof(info);
    if(!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&info,sizeof(info))) {
        fprintf(stderr,"GetProcessMemoryInfo failed: %lu\n",GetLastError()); exit(1);
    }
    return (uint64_t)info.PrivateUsage;
}
uint64_t uncharted_uwp_app_memory_limit(void) { return UINT64_C(5120)<<20; }
void uncharted_uwp_pager_thread_init(void) {}
#ifdef UNCHARTED_RESOLVER_CONTRACT
extern int uncharted_renderer_contract(void);
int main(void) { return uncharted_renderer_contract(); }
#else
int main(int argc,char **argv) {
    if(argc!=2) { fputs("Usage: uncharted_memory_contract <existing private work directory>\n",stderr); return 1; }
    char swap[32768];
    if(strlen(argv[1])+32>=sizeof(swap)) return 1;
    snprintf(swap,sizeof(swap),"%s/uncharted-memory-contract-swap.bin",argv[1]);
    if(_putenv_s("BB_GPU_USER_DIR",argv[1]) || _putenv_s("BB_UWP_SWAP_FILE",swap)) return 1;
    puts("Desktop Windows test of UWP memory APIs; policy budget=5 GiB, no enforced OS memory cap");
    char *arguments[]={"uncharted-memory-contract","--memory-selftest"};
    const int result=uncharted_probe_main(2,arguments);
    printf("Memory contract result: %d\n",result); fflush(NULL);
    return result;
}

#endif
