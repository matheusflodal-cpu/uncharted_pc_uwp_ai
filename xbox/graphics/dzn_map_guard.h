/* SPDX-License-Identifier: MIT */
#ifndef UNCHARTED_DZN_MAP_GUARD_H
#define UNCHARTED_DZN_MAP_GUARD_H
#include <stdint.h>
void dznUwpReportCommandError(const char *phase,uint32_t status);
int32_t dznUwpDiagnosticMapFailure(void);
// Vulkan result values; kept standalone so the failure decision is tested
// without D3D12. A failed map is never converted to a successful command.
static int32_t dzn_uwp_check_map(int32_t status,const void *pointer,int32_t removed) {
    if(status>=0 && pointer) return 0;
    dznUwpReportCommandError("command_resource_map",(uint32_t)status);
    dznUwpReportCommandError("command_device_removed_reason",(uint32_t)removed);
    return removed<0?-4:-5; // DEVICE_LOST / MEMORY_MAP_FAILED
}
#endif
