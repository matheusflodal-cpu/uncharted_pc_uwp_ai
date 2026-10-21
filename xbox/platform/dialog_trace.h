/* SPDX-License-Identifier: GPL-2.0-or-later
 * Diagnostic prefix layouts checked against shadPS4 system/msgdialog_ui.h.
 * Observation only: do not change the guest dialog status or result. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef int (*UnchartedDialogRead)(void*,const void*,void*,size_t);
typedef struct {
    uint64_t base_size; unsigned char base_rest[40];
    uint64_t size; uint32_t mode,pad;
    uint64_t user,progress,system;
} UnchartedDialogPrefix;
_Static_assert(sizeof(UnchartedDialogPrefix)==88,"MsgDialog prefix ABI");
static int uncharted_dialog_trace(FILE* out,const void* param,UnchartedDialogRead read,void* context) {
    UnchartedDialogPrefix p;
    if(!param || !read(context,param,&p,sizeof(p))) {
        fputs("UWP MsgDialog trace: unreadable parameter\n",out);return 0;
    }
    fprintf(out,"UWP MsgDialog trace: base_size=%llu size=%llu mode=%u\n",
        (unsigned long long)p.base_size,(unsigned long long)p.size,p.mode);
    if(p.base_size!=48 || p.size!=136) {
        fputs("UWP MsgDialog trace: layout not recognized; pointers not followed\n",out);return 0;
    }
    if(p.mode==3) {
        uint32_t type;
        if(p.system && read(context,(const void*)(uintptr_t)p.system,&type,sizeof(type))) {
            fprintf(out,"UWP MsgDialog system message: type=%u\n",type);return 1;
        }
        fputs("UWP MsgDialog trace: unreadable system message\n",out);return 0;
    }
    if(p.mode!=1 && p.mode!=2) {fputs("UWP MsgDialog trace: unknown mode\n",out);return 0;}
    const uint64_t pointer=p.mode==1 ? p.user : p.progress;
    struct {uint32_t type,pad;uint64_t message;} sub;
    if(!pointer || !read(context,(const void*)(uintptr_t)pointer,&sub,sizeof(sub))) {
        fputs("UWP MsgDialog trace: unreadable message parameter\n",out);return 0;
    }
    char text[1025];size_t n=0;int ended=0;
    for(;n<sizeof(text)-1;++n) {
        unsigned char c;
        if(sub.message>UINTPTR_MAX-n || !read(context,(const void*)(uintptr_t)(sub.message+n),&c,1)) {
            fputs("UWP MsgDialog trace: unreadable message string\n",out);return 0;
        }
        if(!c) {ended=1;break;}
        text[n]=c<32 || c==127 ? ' ' : (char)c;
    }
    text[n]=0;
    fprintf(out,"UWP MsgDialog %s: type=%u text=%s%s\n",p.mode==1?"user":"progress",sub.type,text,ended?"":" [truncated]");
    return 1;
}
