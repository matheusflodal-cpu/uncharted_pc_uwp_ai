/* SPDX-License-Identifier: GPL-2.0-or-later
 * CPU-loader milestone only. Unported guest imports remain unresolved, so the
 * loader names them instead of pretending graphics, audio or input succeeded.
 * This file must be removed from the link when real backends are connected.
 */
#include "runtime.h"
#include "gpu/bbgpu.h"
#include <stdio.h>
#define UNPORTED(service) uintptr_t runtime_##service##_resolve(const char *nid) { (void)nid; return 0; }
#ifndef UNCHARTED_GUEST_AUDIO
UNPORTED(audio)
UNPORTED(ajm)
void runtime_audio_report(void) { puts("UWP diagnostic: audio backend not connected"); }
void runtime_ajm_report(void) { puts("UWP diagnostic: decoder backend not connected"); }
#endif
#ifndef UNCHARTED_GUEST_INPUT
UNPORTED(pad)
#endif
#ifndef UNCHARTED_GUEST_INPUT
void runtime_pad_report(void) { puts("UWP diagnostic: guest controller backend not connected"); }
#endif
const RuntimeGuestCallback *runtime_guest_callbacks(size_t *count) { *count=0; return NULL; }
#ifndef UNCHARTED_LINK_RENDERER
void bbgpu_register_kernel(void) { puts("UWP diagnostic: GPU/event queues not connected"); }
#endif
void bbnet_configure(const char *app0,const char *serial,const char *title,const char *version) {
    (void)app0; (void)serial; (void)title; (void)version;
}
#ifndef UNCHARTED_LINK_RENDERER
int bbgpu_init(const BbGpuConfig *config) { (void)config; return -1; }
uintptr_t bbgpu_resolve(const char *name) { (void)name; return 0; }
int bbgpu_handle_fault(void *context,void *address) { (void)context; (void)address; return 0; }
void bbgpu_dump_guest_writes(void *context) { (void)context; }
unsigned bbgpu_symbol_count(void) { return 0; }
int bbgpu_text_input_begin(const char *text,const char *prompt) { (void)text; (void)prompt; return 0; }
int bbgpu_text_input_poll(char *text,uint64_t size) { (void)text; (void)size; return 2; }
int bbgpu_overlay_captures_input(void) { return 0; }
void bbgpu_mouse_configure(int look) { (void)look; }
int bbgpu_camera_yaw(float *yaw,double *age) { (void)yaw; (void)age; return 0; }
int bbgpu_mouse_sample(float *dx,float *dy,uint32_t *buttons,int *wheel) {
    *dx=0; *dy=0; *buttons=0; *wheel=0; return 0;
}
#endif
int vulkan_smoke(void) { fputs("STOP: this milestone contains no Vulkan renderer\n",stderr); return 21; }
