// SPDX-License-Identifier: GPL-2.0-or-later
#include <unknwn.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Popups.h>
#include <windows.h>
#include <fileapifromapp.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

extern "C" int uncharted_probe_main(int argc,char **argv);
extern "C" FILE *uncharted_uwp_fopen(const char*,const char*);
#ifdef UNCHARTED_GRAPHICS_PROBE
#include "present_bridge.h"
static winrt::agile_ref<winrt::Windows::UI::Core::CoreWindow> core_window;
extern "C" IUnknown* uncharted_uwp_core_window() {
    auto window=core_window.get();
    auto result=reinterpret_cast<IUnknown*>(winrt::get_abi(window));
    result->AddRef();
    return result;
}
#endif
static std::atomic<bool> finished{false};
static std::atomic<bool> closed{false};
extern "C" uint64_t uncharted_uwp_app_memory_usage() {
    return winrt::Windows::System::MemoryManager::AppMemoryUsage();
}
extern "C" uint64_t uncharted_uwp_app_memory_limit() {
    return winrt::Windows::System::MemoryManager::AppMemoryUsageLimit();
}
extern "C" void uncharted_uwp_pager_thread_init() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}
static bool native_smoke() {
    void *code=VirtualAllocFromApp(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!code) return false;
    const unsigned char instructions[]={0xb8,0x2a,0,0,0,0xc3};
    std::memcpy(code,instructions,sizeof(instructions));
    DWORD old=0;
    if(!VirtualProtectFromApp(code,4096,PAGE_EXECUTE_READ,&old)) {
        VirtualFree(code,0,MEM_RELEASE); return false;
    }
    FlushInstructionCache(GetCurrentProcess(),code,4096);
    using Function=int (__attribute__((sysv_abi)) *)();
    const int result=reinterpret_cast<Function>(code)();
    VirtualFree(code,0,MEM_RELEASE);
    std::printf("Executable System V smoke: %s (%d)\n",result==42 ? "PASS" : "FAIL",result);
    return result==42;
}
static std::string line(FILE *file) {
    char buffer[4096]={0};
    if(!file||!std::fgets(buffer,sizeof(buffer),file)) return {};
    buffer[std::strcspn(buffer,"\r\n")]=0;
    return buffer;
}
static int load() {
    const auto local=winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
    const std::string root=winrt::to_string(local);
    FILE *stream=nullptr;
    if(_wfreopen_s(&stream,(std::wstring(local)+L"\\loader.log").c_str(),L"w",stdout)) return 1;
    _wfreopen_s(&stream,(std::wstring(local)+L"\\errors.log").c_str(),L"w",stderr);
    setvbuf(stdout,nullptr,_IONBF,0); setvbuf(stderr,nullptr,_IONBF,0);
#ifdef UNCHARTED_LINK_RENDERER
    std::puts("Uncharted 0.3.6 UWP renderer integration — synthetic shader/texture tests; game execution remains experimental");
#elif defined(UNCHARTED_GRAPHICS_PROBE)
    std::puts("Uncharted 0.3.6 UWP graphics bridge probe — guest renderer and gameplay are not linked");
#else
    std::puts("Uncharted 0.3.6 UWP CPU/memory diagnostic — gameplay is not linked");
#endif
    std::printf("App memory: %llu / %llu MiB\n",uncharted_uwp_app_memory_usage()>>20,
                uncharted_uwp_app_memory_limit()>>20);
#ifdef UNCHARTED_LINK_RENDERER
    _putenv_s("BB_GPU_LOG","info");
    _putenv_s("BB_UPSCALER","off");
    _putenv_s("BB_FRAME_GEN","off");
    _putenv_s("BB_OBJECT_MOTION","0");
#endif
    if(!native_smoke()) { std::puts("STOP: executable-memory smoke failed"); return 1; }
    _putenv_s("BB_GPU_USER_DIR",root.c_str());
    _putenv_s("BB_UWP_SWAP_FILE",(root+"\\uncharted-swap.bin").c_str());
    FILE *settings=uncharted_uwp_fopen((root+"\\run.txt").c_str(),"rb");
    std::string mode=line(settings),dump=line(settings),game=line(settings);
    if(settings) std::fclose(settings);
#ifdef UNCHARTED_GRAPHICS_PROBE
#ifdef UNCHARTED_LINK_RENDERER
    if(mode.empty()||mode=="renderer"||mode=="graphics") {
        std::puts("Renderer texture/shader self-test starting");
        int result=UnchartedUwp::TextureProbe();
        std::printf("Renderer texture/shader self-test: %s\n",result ? "FAIL" : "PASS");
        return result ? result : UnchartedUwp::GraphicsProbe();
    }
#endif
    if(mode.empty()||mode=="graphics") return UnchartedUwp::GraphicsProbe();
#endif
    if(mode.empty()||mode=="memory") {
        std::vector<std::string> values={"uncharted-uwp","--memory-selftest"};
        std::vector<char*> args; for(auto &v:values) args.push_back(v.data());
        return uncharted_probe_main(int(args.size()),args.data());
    }
    if(mode!="imports"&&mode!="boot") { std::puts("STOP: run.txt mode must be memory, imports or boot"); return 1; }
    if(game!="1"&&game!="2"&&game!="3") { std::puts("STOP: run.txt line 3 must select game 1, 2 or 3"); return 1; }
    if(dump.empty()) { std::puts("STOP: run.txt line 2 needs the dump folder"); return 1; }
    std::string prepared=root+"\\Prepared\\u"+game;
    const std::string user=root+"\\user";
    CreateDirectoryFromAppW(winrt::to_hstring(user).c_str(),nullptr);
    std::vector<std::string> values={"uncharted-uwp",prepared+"\\boot-linked.bin","--cpu-only",
        "--app0",dump,"--user",user,"--content-profile",prepared+"\\content.bin","--timeout","0"};
#ifdef UNCHARTED_LINK_RENDERER
    if(mode=="boot") {
        values.erase(std::find(values.begin(),values.end(),"--cpu-only"));
        std::puts("EXPERIMENTAL guest boot: renderer linked; audio, controller and online guest APIs remain unavailable");
    }
#endif
    if(mode=="imports") values.push_back("--list-missing");
    std::puts("Unported guest APIs stop with a named missing import.");
    std::vector<char*> args; for(auto &v:values) args.push_back(v.data());
    return uncharted_probe_main(int(args.size()),args.data());
}
struct View : winrt::implements<View,winrt::Windows::ApplicationModel::Core::IFrameworkView> {
    winrt::Windows::UI::Core::CoreWindow window{nullptr};
    void Initialize(winrt::Windows::ApplicationModel::Core::CoreApplicationView const& app) {
        app.Activated([this](auto const&,auto const&) { window.Activate(); });
    }
    void SetWindow(winrt::Windows::UI::Core::CoreWindow const& value) {
        window=value;
#ifdef UNCHARTED_GRAPHICS_PROBE
        core_window=winrt::make_agile(window);
#endif
        window.Closed([](auto const&,auto const&) { closed=true; TerminateProcess(GetCurrentProcess(),0); });
    }
    void Load(winrt::hstring const&) {}
    void Run() {
        auto dispatcher=window.Dispatcher();
        std::thread([dispatcher] {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            int result=1;
            try { result=load(); }
            catch(std::exception const& e) { std::fprintf(stderr,"STOP: %s\n",e.what()); }
            catch(...) { std::fputs("STOP: unhandled host error\n",stderr); }
            std::printf("Diagnostic finished: %d\n",result); std::fflush(nullptr);
            finished=true;
            dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[result] {
                winrt::Windows::UI::Popups::MessageDialog dialog(
                    winrt::to_hstring("Diagnostic result: "+std::to_string(result)+
                    ". Retrieve LocalState/loader.log and errors.log through Device Portal. Gameplay is not implemented."),
                    L"Uncharted UWP Diagnostic");
                dialog.ShowAsync().Completed([](auto const&,auto) {
                    winrt::Windows::ApplicationModel::Core::CoreApplication::Exit();
                });
            });
        }).detach();
        std::thread([] {
            for(int i=0;i<600&&!finished&&!closed;++i) Sleep(100);
            if(!finished&&!closed) { std::fputs("STOP: 60-second diagnostic watchdog\n",stderr); std::fflush(nullptr); TerminateProcess(GetCurrentProcess(),124); }
        }).detach();
        while(!closed) window.Dispatcher().ProcessEvents(winrt::Windows::UI::Core::CoreProcessEventsOption::ProcessOneAndAllPending);
    }
    void Uninitialize() {}
};
struct ViewSource : winrt::implements<ViewSource,winrt::Windows::ApplicationModel::Core::IFrameworkViewSource> {
    winrt::Windows::ApplicationModel::Core::IFrameworkView CreateView() { return winrt::make<View>(); }
};
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    winrt::init_apartment();
    winrt::Windows::ApplicationModel::Core::CoreApplication::Run(winrt::make<ViewSource>());
    return 0;
}
