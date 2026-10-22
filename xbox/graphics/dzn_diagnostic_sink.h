/* SPDX-License-Identifier: MIT
 * Dozen-to-host diagnostics. Register once before Vulkan device creation;
 * callback can run on concurrent compiler threads and must not call Vulkan.
 */
#ifndef UNCHARTED_DZN_DIAGNOSTIC_SINK_H
#define UNCHARTED_DZN_DIAGNOSTIC_SINK_H
#include <stdint.h>
#include <stdio.h>
typedef void (*DznUwpDiagnosticCallback)(const char *message);
static DznUwpDiagnosticCallback dzn_uwp_sink;
// Host compiler contract only. The app never calls this export; zero keeps
// the adapter's real shader-model selection. Set once before device/workers.
static int dzn_uwp_legacy_shader_model;
#ifdef _WIN32
__declspec(dllexport)
#endif
void dznSetUwpDiagnosticLegacyShaderModel(int enabled) {
    dzn_uwp_legacy_shader_model=enabled!=0;
}
#ifdef _WIN32
__declspec(dllexport)
#endif
void dznSetUwpDiagnosticCallback(DznUwpDiagnosticCallback callback) {
    dzn_uwp_sink=callback;
}
static void dzn_uwp_report(const char *phase,uint32_t detail,const char *message) {
    if(!dzn_uwp_sink) return;
    char line[2048];
    snprintf(line,sizeof(line),"Dozen diagnostic: phase=%.96s detail=0x%08x message=%.1600s",
             phase?phase:"unknown",(unsigned)detail,message?message:"");
    dzn_uwp_sink(line);
}
// Fault injection is confined to the desktop command contract; off by default.
static int32_t dzn_uwp_map_failure;
#ifdef _WIN32
__declspec(dllexport)
#endif
void dznSetUwpDiagnosticMapFailure(int32_t status) { dzn_uwp_map_failure=status; }
int32_t dznUwpDiagnosticMapFailure(void) {return dzn_uwp_map_failure;}
void dznUwpReportCommandError(const char *phase,uint32_t status) {
    dzn_uwp_report(phase,status,"command recording failure; error preserved");
}
#endif
