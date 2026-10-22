"""Add an MIT diagnostic callback to one pinned Dozen source, preserving errors."""
from pathlib import Path
import hashlib
import sys

EXPECTED = '45b70d383c75dfe6592c24ca9d478fe61587a65e111e04fde3fa0504c1657b50'
MARKER = '#include "dzn_uwp_diagnostics.h"'
HEADER = Path(__file__).resolve().parents[1] / 'graphics/dzn_diagnostic_sink.h'

def patch(source):
    if hashlib.sha256(source.encode()).hexdigest() != EXPECTED:
        raise ValueError('Dozen diagnostics requires the exact pinned dzn_pipeline.c source')
    edits = [
        ('.shader_model_max = dzn_get_shader_model(pdev),',
         '.shader_model_max = dzn_uwp_legacy_shader_model ? SHADER_MODEL_6_0 : dzn_get_shader_model(pdev),'),
        ('#include "dzn_private.h"', '#include "dzn_private.h"\n'+MARKER),
        ('   } else {\n      result = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);',
         '   } else {\n      dzn_uwp_report("nir_to_dxil", (uint32_t)nir->info.stage, "conversion failed");\n      result = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);'),
        ('   if (!res && !(instance->debug_flags & DZN_DEBUG_EXPERIMENTAL)) {',
         '   if (!res && !(instance->debug_flags & DZN_DEBUG_EXPERIMENTAL)) {\n      dzn_uwp_report("dxil_validation", (uint32_t)nir->info.stage, err);'),
        ('   if (!pipeline)\n      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);',
         '   if (!pipeline) {\n      dzn_uwp_report("pipeline_host_allocation", (uint32_t)sizeof(*pipeline), "vk_zalloc2 failed");\n      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);\n   }'),
        ('      if (FAILED(hres)) {\n         ret = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);',
         '      if (FAILED(hres)) {\n         dzn_uwp_report("d3d12_graphics_pipeline", (uint32_t)hres, "CreatePipelineState failed");\n         dzn_uwp_report("device_removed_reason", (uint32_t)ID3D12Device_GetDeviceRemovedReason(device->dev), "after failed graphics pipeline");\n         ret = vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);'),
        ('   if (ret != VK_SUCCESS) {\n      dzn_graphics_pipeline_destroy(pipeline, pAllocator);',
         '   if (ret != VK_SUCCESS) {\n      dzn_uwp_report("graphics_pipeline_exit", (uint32_t)ret, "failure preserved");\n      dzn_graphics_pipeline_destroy(pipeline, pAllocator);'),
    ]
    for old,new in edits:
        count=source.count(old)
        # Graphics and compute allocate the same way; instrument both allocations.
        expected=2 if old.startswith('   if (!pipeline)') or old.startswith('.shader_model_max') else 1
        if count!=expected: raise ValueError(f'unexpected patch context count {count}, expected {expected}')
        source=source.replace(old,new)
    return source

COMMAND_EXPECTED='5c53dfc76bebf8b216008114052cda6404edfc6c6026a885473f440e65edf137'
DEVICE_REPORT = r"""
#ifdef _WIN32
__declspec(dllexport) void dznReportUwpDeviceStatus(VkDevice handle) {
   VK_FROM_HANDLE(dzn_device, device, handle);
   dzn_uwp_report("device_removed_reason", (uint32_t)ID3D12Device_GetDeviceRemovedReason(device->dev), "explicit device status");
   ID3D12DeviceRemovedExtendedData *dred = NULL;
   HRESULT status = IUnknown_QueryInterface((IUnknown *)device->dev, &IID_ID3D12DeviceRemovedExtendedData, (void **)&dred);
   dzn_uwp_report("dred_interface", (uint32_t)status, "optional native device diagnostics");
   if (FAILED(status) || !dred) return;
   D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT output = {0};
   status = ID3D12DeviceRemovedExtendedData_GetAutoBreadcrumbsOutput(dred, &output);
   dzn_uwp_report("dred_breadcrumbs", (uint32_t)status, "native breadcrumb query");
   if (SUCCEEDED(status)) {
      const D3D12_AUTO_BREADCRUMB_NODE *node = output.pHeadAutoBreadcrumbNode;
      unsigned index = 0;
      for (; node && index < 32; node = node->pNext, ++index) {
         uint32_t completed = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
         char detail[384];
         snprintf(detail, sizeof(detail), "node=%u list=%p queue=%p completed=%u recorded=%u sampled=%s",
                  index, (void *)node->pCommandList, (void *)node->pCommandQueue,
                  completed, node->BreadcrumbCount, node->pLastBreadcrumbValue ? "yes" : "no");
         dzn_uwp_report("dred_node", index, detail);
         /* Avoid interpreting overwritten ring entries or unsampled counters.
          * Small command lists have a complete, directly indexed history. */
         if (node->pCommandHistory && node->pLastBreadcrumbValue &&
             node->BreadcrumbCount <= 65536 && completed < node->BreadcrumbCount) {
            dzn_uwp_report("dred_next_operation", (uint32_t)node->pCommandHistory[completed], "pending operation; not proof of the faulting instruction");
         }
      }
      if (node) dzn_uwp_report("dred_truncated", index, "bounded to 32 command lists");
   }
   D3D12_DRED_PAGE_FAULT_OUTPUT fault = {0};
   status = ID3D12DeviceRemovedExtendedData_GetPageFaultAllocationOutput(dred, &fault);
   char address[80];
   snprintf(address, sizeof(address), "page_fault_va=0x%llx", (unsigned long long)fault.PageFaultVA);
   dzn_uwp_report("dred_page_fault", (uint32_t)status, address);
   ID3D12DeviceRemovedExtendedData_Release(dred);
}
#endif
"""

def patch_commands(source):
    if hashlib.sha256(source.encode()).hexdigest()!=COMMAND_EXPECTED:
        raise ValueError('Dozen diagnostics requires the exact pinned dzn_cmd_buffer.c')
    source=source.replace('#include "dzn_private.h"','#include "dzn_private.h"\n#include "dzn_uwp_map_guard.h"',1)
    # Only the two known transfer commands are changed. Keep their original
    # successful mapping, copy, unmap and command-list recording paths intact.
    for function,next_function,decl,cast in [
        ('dzn_CmdFillBuffer(', 'dzn_CmdUpdateBuffer(', 'uint32_t *cpu_ptr;', '(void **)&cpu_ptr'),
        ('dzn_CmdUpdateBuffer(', 'dzn_CmdClearAttachments(', 'void *cpu_ptr;', '&cpu_ptr'),
    ]:
        start=source.index(function);end=source.index(next_function,start)
        body=source[start:end]
        old=decl+'\n   ID3D12Resource_Map(src_res, 0, NULL, '+cast+');'
        new=decl.replace(';',' = NULL;')+r"""
   HRESULT map_status = (HRESULT)dznUwpDiagnosticMapFailure();
   if (SUCCEEDED(map_status))
      map_status = ID3D12Resource_Map(src_res, 0, NULL, """+cast+r""");
   if (FAILED(map_status) || !cpu_ptr) {
      struct dzn_device *device = container_of(cmdbuf->vk.base.device, struct dzn_device, vk);
      HRESULT removed = ID3D12Device_GetDeviceRemovedReason(device->dev);
      VkResult failure = (VkResult)dzn_uwp_check_map((int32_t)map_status, cpu_ptr, (int32_t)removed);
      vk_command_buffer_set_error(&cmdbuf->vk, failure);
      return;
   }"""
        if body.count(old)!=1:raise ValueError('unexpected command mapping context')
        source=source[:start]+body.replace(old,new,1)+source[end:]
    return source

def main():
    root=Path(sys.argv[1]);directory=root/'src/microsoft/vulkan'
    paths=[directory/'dzn_pipeline.c',directory/'dzn_cmd_buffer.c']
    originals=[p.read_text(encoding='utf-8') for p in paths]
    results=[patch(originals[0])+DEVICE_REPORT,patch_commands(originals[1])]
    if len(sys.argv)>2 and sys.argv[2]=='--check':
        for original,result,apply in zip(originals,results,[patch,patch_commands]):
            for invalid in [original+'\n',result,original.replace('dzn_private.h','wrong_private.h')]:
                try:apply(invalid)
                except ValueError:pass
                else:raise AssertionError('changed or already patched source was accepted')
        print('Dozen patch contract: PASS; both exact pins, command guard and rejection of changed/reapplied source')
        return
    for path,result in zip(paths,results):path.write_text(result,encoding='utf-8',newline='\n')
    for name,dest in [('dzn_diagnostic_sink.h','dzn_uwp_diagnostics.h'),('dzn_map_guard.h','dzn_uwp_map_guard.h')]:
        (directory/dest).write_text((HEADER.parent/name).read_text(),encoding='utf-8',newline='\n')
    print('Applied pipeline/device diagnostics and checked transfer maps to pinned Dozen')
if __name__=='__main__':main()
