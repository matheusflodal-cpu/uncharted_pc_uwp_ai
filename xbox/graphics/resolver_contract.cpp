// SPDX-License-Identifier: GPL-2.0-or-later
// Desktop test: resolve the actual reported graphics imports before any GPU/window exists.
#include <unknwn.h>
#include <cstdio>
#include <cstdlib>
#include "gpu/bbgpu.h"
extern "C" {
#include "runtime.h"
}
namespace Frontend { class WindowSDL; }
extern Frontend::WindowSDL* g_window;
extern "C" IUnknown* uncharted_uwp_core_window() {
    std::fputs("FAIL: headless import check requested a CoreWindow\n",stderr);
    std::exit(91);
}
extern "C" int uncharted_renderer_contract() {
    struct Import { const char* nid; const char* name; };
    static const Import imports[]={
    {"DYhhWbJSeRg#C#D","sceVideoOutColorSettingsSetGamma_"},
    {"pv9CI5VC+R0#C#D","sceVideoOutAdjustColor_"},
    {"6kPnj51T62Y#C#D","sceVideoOutGetResolutionStatus"},
    {"HXzjK9yI30k#C#D","sceVideoOutAddFlipEvent"},
    {"Up36PTk687E#C#D","sceVideoOutOpen"},
    {"CBiu4mCE1DA#C#D","sceVideoOutSetFlipRate"},
    {"i6-sR91Wt-4#C#D","sceVideoOutSetBufferAttribute"},
    {"w3BY+tAEiQY#C#D","sceVideoOutRegisterBuffers"},
    {"0H2vBYbTLHI#B#C","sceGnmDrawInitDefaultHardwareState200"},
    {"bQVd5YzCal0#B#C","sceGnmSetPsShader"},
    {"X9Omw9dwv5M#B#C","sceGnmSetEmbeddedPsShader"},
    {"4MgRw-bVNQU#B#C","sceGnmUpdatePsShader"},
    {"gAhCn6UiU4Y#B#C","sceGnmSetVsShader"},
    {"+AFvOEXrKJk#B#C","sceGnmSetEmbeddedVsShader"},
    {"V31V01UiScY#B#C","sceGnmUpdateVsShader"},
    {"FUHG8sQ3R58#B#C","sceGnmSetEsShader"},
    {"VJNjFtqiF5w#B#C","sceGnmSetHsShader"},
    {"GNlx+y7xPdE#B#C","sceGnmUpdateHsShader"},
    {"vckdzbQ46SI#B#C","sceGnmSetLsShader"},
    {"KXltnCwEJHQ#B#C","sceGnmSetCsShader"},
    {"UJwNuMBcUAk#B#C","sceGnmSetGsShader"},
    {"nLM2i2+65hA#B#C","sceGnmUpdateGsShader"},
    {"jiItzS6+22g#B#C","sceGnmInsertSetMarker"},
    {"aj3L-iaFmyk#B#C","sceGnmInsertSetColorMarker"},
    {"W1Etj-jlW7Y#B#C","sceGnmInsertPushMarker"},
    {"aPIZJTXC+cU#B#C","sceGnmInsertPushColorMarker"},
    {"7qZVNgEu+SY#B#C","sceGnmInsertPopMarker"},
    {"1qXLHIpROPE#B#C","sceGnmInsertWaitFlipDone"},
    {"26PM5Mzl8zc#B#C","sceGnmLogicalCuIndexToPhysicalCuIndex"},
    {"RU74kek-N0c#B#C","sceGnmLogicalCuMaskToPhysicalCuMask"},
    {"zwY0YV91TTI#B#C","sceGnmSubmitCommandBuffers"},
    {"xbxNatawohc#B#C","sceGnmSubmitAndFlipCommandBuffers"},
    {"b0xyllnVY-I#B#C","sceGnmAddEqEvent"},
    {"PVT+fuoS9gU#B#C","sceGnmDeleteEqEvent"},
    {"UoYY0DWMC0U#B#C","sceGnmGetEqEventType"},
    {"H7-fgvEutM0#B#C","sceGnmGetEqTimeStamp"},
    {"29oKvKXzEZo#B#C","sceGnmMapComputeQueue"},
    {"A+uGq+3KFtQ#B#C","sceGnmMapComputeQueueWithPriority"},
    {"ArSg-TGinhk#B#C","sceGnmUnmapComputeQueue"},
    {"bX5IbRvECXk#B#C","sceGnmDingDong"},
    {"iBt3Oe00Kvc#B#C","sceGnmFlushGarlic"},
    {"qpGITzPE+Zc#B#C","sceGnmDebugHardwareStatus"},
    {"yvZ73uQUqrk#B#C","sceGnmSubmitDone"},
    {"b08AgtPlHPg#B#C","sceGnmAreSubmitsAllowed"},
    {"gObODli-OH8#B#C","sceGnmRequestFlipAndSubmitDone"},
    {"+xuDhxlWRPg#B#C","sceGnmSetupMipStatsReport"},
    {"f85orjx7qts#B#C","sceGnmRequestMipStatsReportAndReset"},
    {"HHo1BAljZO8#B#C","sceGnmDisableMipStatsReport"},
    {"jtkqXpAOY6w#B#C","sceGnmSetGsRingSizes"},
    {"ln33zjBrfjk#B#C","sceGnmGetTheTessellationFactorRingBufferBaseAddress"},
    {"HzMN7ANqYEc#B#C","sceGnmValidateGetVersion"},
    {"rTIV11nMQuM#B#C","sceGnmValidateOnSubmitEnabled"},
    {"MBMa6EFu4Ko#B#C","sceGnmValidateResetState"},
    {"iCO804ZgzdA#B#C","sceGnmValidateCommandBuffers"},
    {"Q7t4VEYLafI#B#C","sceGnmValidationRegisterMemoryCheckCallback"},
    {"f33OrruQYbM#B#C","sceRazorIsLoaded"},
    {"u9YKpRRHe-M#B#C","sceRazorCaptureImmediate"},
    {"4UFagYlfuAM#B#C","sceRazorCaptureSinceLastFlip"},
    {"xeTLfxVIQO4#B#C","sceRazorCaptureCommandBuffersOnlyImmediate"},
    {"9thMn+uB1is#B#C","sceRazorCaptureCommandBuffersOnlySinceLastFlip"},
    {"d88anrgNoKY#B#C","sceGnmDriverTriggerCapture"},
    {"TLV4mswiZ4A#B#C","sceGnmDriverCaptureInProgress"},
    {"R6z1xM3pW-w#B#C","sceGnmDriverTraceInProgress"},
    {"jg33rEKLfVs#B#C","sceGnmIsUserPaEnabled"},
    {"nF6bFRUBRAU#B#C","sceGnmDispatchInitDefaultHardwareState"},
    };
    if(g_window) return 1;
    runtime_start(1);
    bbgpu_register_kernel();
    unsigned missing=0;
    for(const auto& entry:imports) if(!runtime_resolve(entry.nid,0)) {
        std::fprintf(stderr,"FAIL: missing %s %s\n",entry.nid,entry.name);
        ++missing;
    }
    if(g_window||runtime_resolve("unknown#B#C",0)||runtime_resolve(imports[0].nid,1)) return 2;
    // Unimplemented guest audio and input must remain unresolved.
    if(runtime_resolve("hv1luiJrqQM#A#B",0)||runtime_resolve("JfEPXVxhFqA#G#H",0)) return 3;
    std::printf("Graphics import contract: checked=%zu missing=%u window=none\n",sizeof(imports)/sizeof(imports[0]),missing);
    return missing ? 4 : 0;
}
