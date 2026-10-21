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

def main():
    root=Path(sys.argv[1]);path=root/'src/microsoft/vulkan/dzn_pipeline.c'
    original=path.read_text(encoding='utf-8')
    result=patch(original)
    if len(sys.argv)>2 and sys.argv[2]=='--check':
        for invalid in [original+'\n', result, original.replace('vk_zalloc2','vk_zalloc3')]:
            try:patch(invalid)
            except ValueError:pass
            else:raise AssertionError('changed or already patched source was accepted')
        print('Dozen patch contract: PASS; exact pin, contexts and rejection of changed/reapplied source')
        return
    path.write_text(result,encoding='utf-8',newline='\n')
    (path.parent/'dzn_uwp_diagnostics.h').write_text(HEADER.read_text(),encoding='utf-8',newline='\n')
    print('Applied diagnostic callback to pinned Dozen; original failure handling preserved')
if __name__=='__main__':main()
