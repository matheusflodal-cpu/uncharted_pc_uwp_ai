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
#include <future>
#include <memory>
#include <stdexcept>
#include "prepare_game.h"
#include "boot_trace.h"
#include "runtime_wait_trace.h"
#include <string>
#include <thread>
#include <vector>

extern "C" int uncharted_probe_main(int argc,char **argv);
extern "C" void runtime_file_profile_report(void);
#ifdef UNCHARTED_GUEST_INPUT
#include "input.h"
int UnchartedInputProbe();
int UnchartedAudioProbe();
extern "C" void runtime_pad_boot_snapshot(FILE*);
#endif
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
static bool trace_guest_tasks=false;
static std::atomic<bool> finished{false};
static std::atomic<bool> closed{false};
static std::atomic<ULONGLONG> deadline{0};
static std::atomic<const char*> phase{"startup"};
static void watch_for(unsigned seconds,const char* name="diagnostic") { phase=name; deadline=GetTickCount64()+ULONGLONG(seconds)*1000; }

// Wait on a worker; all popup creation and callbacks stay on the UI dispatcher.
static int choose(const winrt::Windows::UI::Core::CoreDispatcher& dispatcher,
                  const wchar_t* title,const wchar_t* message,std::vector<std::wstring> labels,
                  bool cancellable=true) {
    auto answer=std::make_shared<std::promise<int>>();
    auto future=answer->get_future();
    dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
        [answer,title,message,labels=std::move(labels),cancellable] {
        try {
            using namespace winrt::Windows::UI::Popups;
            MessageDialog dialog(message,title);
            for(size_t i=0;i<labels.size();++i)
                dialog.Commands().Append(UICommand(labels[i],nullptr,winrt::box_value(int32_t(i))));
            dialog.DefaultCommandIndex(0);
            if(cancellable) dialog.CancelCommandIndex(uint32_t(labels.size()-1));
            dialog.ShowAsync().Completed([answer](auto const& operation,auto) {
                try {
                    auto command=operation.GetResults();
                    answer->set_value(command ? winrt::unbox_value<int32_t>(command.Id()) : -1);
                } catch(...) { answer->set_exception(std::current_exception()); }
            });
        } catch(...) { answer->set_exception(std::current_exception()); }
    });
    return future.get();
}
static UnchartedPrepare::Bytes read_input(const std::string& path,size_t limit) {
    FILE* raw=uncharted_uwp_fopen(path.c_str(),"rb");
    if(!raw) throw std::runtime_error("Cannot open required input: "+path);
    std::unique_ptr<FILE,decltype(&std::fclose)> file(raw,std::fclose);
    if(_fseeki64(raw,0,SEEK_END)) throw std::runtime_error("Cannot seek: "+path);
    auto size=_ftelli64(raw);
    if(size<=0||uint64_t(size)>limit) throw std::runtime_error("Empty or oversized input: "+path);
    if(_fseeki64(raw,0,SEEK_SET)) throw std::runtime_error("Cannot rewind: "+path);
    UnchartedPrepare::Bytes bytes(size_t(size),0);
    if(std::fread(bytes.data(),1,bytes.size(),raw)!=bytes.size()) throw std::runtime_error("Short read: "+path);
    return bytes;
}
static void publish(const std::string& path,const UnchartedPrepare::Bytes& bytes) {
    const auto temporary=winrt::to_hstring(path+".tmp"), destination=winrt::to_hstring(path);
    FILE* raw=uncharted_uwp_fopen((path+".tmp").c_str(),"wb");
    if(!raw) throw std::runtime_error("Cannot create preparation: "+path);
    bool ok=std::fwrite(bytes.data(),1,bytes.size(),raw)==bytes.size();
    if(std::fflush(raw)) ok=false;
    if(std::fclose(raw)) ok=false;
    if(!ok) { DeleteFileFromAppW(temporary.c_str()); throw std::runtime_error("Short write: "+path); }
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    bool moved=!GetFileAttributesExFromAppW(destination.c_str(),GetFileExInfoStandard,&attributes)
        ? MoveFileFromAppW(temporary.c_str(),destination.c_str())!=FALSE
        : ReplaceFileFromAppW(destination.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr)!=FALSE;
    if(!moved) { DeleteFileFromAppW(temporary.c_str()); throw std::runtime_error("Cannot publish preparation: "+path); }
}
static bool is_dump(const std::string& path) {
    try {
        auto bytes=read_input(path+"\\sce_sys\\param.sfo",1<<20);
        return UnchartedPrepare::CheckTitle(bytes).empty();
    } catch(...) { return false; }
}
static std::string discover_dump(const std::string& root) {
    std::vector<std::string> candidates={root+"\\Dump"}, volumes;
    try {
        auto devices=winrt::Windows::Storage::KnownFolders::RemovableDevices().GetFoldersAsync().get();
        for(auto const& volume:devices) {
            const auto path=winrt::to_string(volume.Path());
            std::printf("USB volume: %s\n",path.c_str());
            if(path.empty()) continue;
            volumes.push_back(path);
        }
    } catch(const winrt::hresult_error& error) {
        std::fprintf(stderr,"USB enumeration: %s\n",winrt::to_string(error.message()).c_str());
    }
    // GoW3's Xbox host also tries E: when WinRT omits the first USB volume.
    if(std::none_of(volumes.begin(),volumes.end(),[](const auto& path) { return _strnicmp(path.c_str(),"E:",2)==0; }))
        volumes.push_back("E:");
    for(auto path:volumes) {
        while(!path.empty()&&(path.back()=='\\'||path.back()=='/')) path.pop_back();
        for(const char* suffix:{"\\dumps\\Uncharted","\\Uncharted"}) candidates.push_back(path+suffix);
    }
    std::string selected;
    for(const auto& path:candidates) if(is_dump(path)) {
        std::printf("Uncharted dump candidate: %s\n",path.c_str());
        if(!selected.empty()) throw std::runtime_error("Multiple dumps found; specify one in LocalState/run.txt");
        selected=path;
    }
    if(selected.empty()) throw std::runtime_error("Dump not found. Place sce_sys, sce_module and game files in USB:\\dumps\\Uncharted, or LocalState\\Dump");
    return selected;
}
static void prepare_dump(const std::string& dump,const std::string& root,const std::string& game) {
    const char* executable=game=="1" ? "eboot.bin" : game=="2" ? "big2-ps4_Shipping.elf" : "big3-ps4_Shipping.elf";
    std::printf("Native preparation starting: game=%s executable=%s dump=%s\n",game.c_str(),executable,dump.c_str());
    UnchartedPrepare::Dump inputs;
    inputs.sfo=read_input(dump+"\\sce_sys\\param.sfo",1<<20);
    const auto title_error=UnchartedPrepare::CheckTitle(inputs.sfo);
    if(!title_error.empty()) throw std::runtime_error(title_error);
    inputs.eboot=read_input(dump+"\\"+executable,512ull<<20);
    inputs.libc=read_input(dump+"\\sce_module\\libc.prx",512ull<<20);
    inputs.fios=read_input(dump+"\\sce_module\\libSceFios2.prx",512ull<<20);
    auto files=UnchartedPrepare::Prepare(inputs);
    const auto directory=root+"\\Prepared", prepared=directory+"\\u"+game;
    for(const auto& path:{directory,prepared})
        if(!CreateDirectoryFromAppW(winrt::to_hstring(path).c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)
            throw std::runtime_error("Cannot create preparation directory: "+path);
    // The loader is called only after both writes succeed. Every boot prepares fresh;
    // it never executes stale cache files after a failed or interrupted preparation.
    publish(prepared+"\\boot-linked.bin",files.boot);
    publish(prepared+"\\content.bin",files.content);
    std::printf("Native preparation: PASS title=%s boot=%zu content=%zu\n",files.title.c_str(),files.boot.size(),files.content.size());
}

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
static int load(const winrt::Windows::UI::Core::CoreDispatcher& dispatcher) {
    const auto local=winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
    const std::string root=winrt::to_string(local);
    FILE *stream=nullptr;
    if(_wfreopen_s(&stream,(std::wstring(local)+L"\\loader.log").c_str(),L"w",stdout)) return 1;
    _wfreopen_s(&stream,(std::wstring(local)+L"\\errors.log").c_str(),L"w",stderr);
    setvbuf(stdout,nullptr,_IONBF,0); setvbuf(stderr,nullptr,_IONBF,0);
#ifdef UNCHARTED_LINK_RENDERER
    std::puts("Uncharted UWP 0.6.12 — UWP RW-to-RX loading diagnostics");
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
    _putenv_s("BB_IO_STATS","1");
    FILE* sync_settings=uncharted_uwp_fopen((root+"\\gpu-sync.txt").c_str(),"rb");
    const std::string sync_mode=line(sync_settings);
    if(sync_settings) std::fclose(sync_settings);
    if(!sync_mode.empty() && sync_mode!="normal" && sync_mode!="conservative")
        throw std::runtime_error("gpu-sync.txt must contain normal or conservative");
    _putenv_s("UNCHARTED_FULL_BARRIERS",sync_mode=="conservative"?"1":"0");
    std::printf("GPU sync mode: %s\n",sync_mode=="conservative"?"conservative":"normal");
    auto read_option=[&](const char* name) {
        FILE* file=uncharted_uwp_fopen((root+"\\"+name).c_str(),"rb");
        const std::string value=line(file);if(file)std::fclose(file);return value;
    };
    const std::string execution=read_option("gpu-execution.txt");
    if(!execution.empty()&&execution!="serial"&&execution!="parallel")
        throw std::runtime_error("gpu-execution.txt must contain serial or parallel");
    const bool serial=execution!="parallel";
    _putenv_s("BB_DRAW_PIPE",serial?"0":"");
    _putenv_s("BB_VK_RECORD_THREAD",serial?"0":"");
    _putenv_s("BB_COPY_THREADS",serial?"0":"");
    std::printf("GPU execution mode: %s\n",serial?"serial":"parallel (upstream defaults)");
    const std::string tasks=read_option("task-trace.txt");
    if(!tasks.empty()&&tasks!="on"&&tasks!="off")
        throw std::runtime_error("task-trace.txt must contain on or off");
    trace_guest_tasks=tasks=="on";
    _putenv_s("UNCHARTED_LOADING_TRACE",trace_guest_tasks?"1":"0");
    std::printf("Guest task tracing: %s\n",trace_guest_tasks?"on":"off");
    const std::string dred=read_option("gpu-dred.txt");
    if(!dred.empty()&&dred!="on"&&dred!="off")
        throw std::runtime_error("gpu-dred.txt must contain on or off");
    _putenv_s("UNCHARTED_DRED",dred=="off"?"0":"1");

#endif
    if(!native_smoke()) { std::puts("STOP: executable-memory smoke failed"); return 1; }
    _putenv_s("BB_GPU_USER_DIR",root.c_str());
    _putenv_s("BB_UWP_SWAP_FILE",(root+"\\uncharted-swap.bin").c_str());
    FILE *settings=uncharted_uwp_fopen((root+"\\run.txt").c_str(),"rb");
    std::string mode=line(settings),dump=line(settings),game=line(settings);
    if(settings) std::fclose(settings);
#ifdef UNCHARTED_LINK_RENDERER
    if(mode.empty()||mode=="menu") {
        int action=choose(dispatcher,L"Uncharted UWP 0.6.12",L"Escolha uma etapa. A inicializacao do jogo ainda e experimental.",
                          {L"Selecionar jogo",L"Diagnosticos",L"Sair"});
        if(action<0||action==2) return 0;
        if(action==1) {
            int diagnostic=choose(dispatcher,L"Diagnosticos",L"Escolha o teste.",
                {L"Testar audio",L"Outros testes",L"Sair"});
            if(diagnostic<0||diagnostic==2) return 0;
            if(diagnostic==0) mode="audio";
            else {
                diagnostic=choose(dispatcher,L"Outros testes",L"Escolha o teste.",
                    {L"Testar controle",L"Testar renderizador",L"Sair"});
                if(diagnostic<0||diagnostic==2) return 0;
                mode=diagnostic==0 ? "pad" : "renderer";
            }
        }
        else {
            int selected=choose(dispatcher,L"Selecionar jogo",L"Qual Uncharted esta no seu dump?",
                                {L"Uncharted 1",L"Uncharted 2",L"Uncharted 3"},false);
            if(selected<0) return 0;
            game=std::to_string(selected+1);
            action=choose(dispatcher,L"Teste com o dump",L"Conferir servicos lista o que falta. Tentar iniciar executa o jogo com o renderizador experimental.",
                          {L"Conferir servicos",L"Tentar iniciar",L"Sair"});
            if(action<0||action==2) return 0;
            mode=action==0 ? "imports" : "boot";
        }
    }
#endif
#ifdef UNCHARTED_GUEST_INPUT
    if(mode=="audio") {
        const int action=choose(dispatcher,L"Teste de audio",L"O teste toca um som baixo por cerca de dois segundos, primeiro a esquerda e depois a direita. Ajuste a TV para um volume confortavel. A tela pode ficar sem imagem.", {L"Comecar",L"Sair"});
        if(action!=0) return 0;
        watch_for(30);
        return UnchartedAudioProbe();
    }
    if(mode=="pad") {
        const int action=choose(dispatcher,L"Teste do controle",L"Depois de Comecar, voce tera 20 segundos numa tela sem imagem. Aperte A, B, X e Y, gire os dois analogicos em circulos e pressione LT e RT ate o fim. Y pede uma vibracao breve. O teste termina sozinho.",
            {L"Comecar",L"Sair"});
        if(action!=0) return 0;
        watch_for(60);
        return UnchartedInputProbe();
    }
#endif
    watch_for(60);
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
    if(mode!="imports"&&mode!="boot") { std::puts("STOP: run.txt mode must be menu, audio, pad, renderer, graphics, memory, imports or boot"); return 1; }
    if(game!="1"&&game!="2"&&game!="3") { std::puts("STOP: run.txt line 3 must select game 1, 2 or 3"); return 1; }
    watch_for(300,"preparation");
    if(dump.empty()) dump=discover_dump(root);
    prepare_dump(dump,root,game);
#ifdef UNCHARTED_GUEST_INPUT
    if(mode=="boot") {
        watch_for(15,"controller readiness");
        const int ready=uncharted_uwp_wait_gamepad(5000);
        if(ready<0) throw std::runtime_error("Falha ao consultar o controle do Xbox. Guarde os logs.");
        if(!ready) throw std::runtime_error("Controle nao detectado antes do boot. Ligue o controle e tente iniciar novamente.");
    }
#endif
    watch_for(mode=="boot" ? 900 : 90,mode=="boot" ? "guest boot" : "imports");
    std::string prepared=root+"\\Prepared\\u"+game;
    const std::string user=root+"\\user";
    CreateDirectoryFromAppW(winrt::to_hstring(user).c_str(),nullptr);
    std::vector<std::string> values={"uncharted-uwp",prepared+"\\boot-linked.bin","--cpu-only",
        "--app0",dump,"--user",user,"--content-profile",prepared+"\\content.bin","--timeout","0"};
#ifdef UNCHARTED_LINK_RENDERER
    if(mode=="boot") {
        UnchartedBootTrace::Enable();
        if(trace_guest_tasks) uncharted_wait_enable();
        std::puts("UWP boot trace enabled: heartbeat every 5 seconds; hard boot limit 900 seconds");
        values.erase(std::find(values.begin(),values.end(),"--cpu-only"));
        std::puts("EXPERIMENTAL guest boot: renderer linked; native Xbox input and XAudio2/AJM linked; online and other system guest APIs remain unavailable");
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
        // Xbox B is a gameplay button; do not let the shell treat it as app navigation.
        winrt::Windows::UI::Core::SystemNavigationManager::GetForCurrentView().BackRequested(
            [](auto const&,const winrt::Windows::UI::Core::BackRequestedEventArgs& event) { event.Handled(true); });
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
            std::string reason;
            try { result=load(dispatcher); }
            catch(const winrt::hresult_error& e) { reason=winrt::to_string(e.message()); std::fprintf(stderr,"STOP: %s\n",reason.c_str()); }
            catch(std::exception const& e) { reason=e.what(); std::fprintf(stderr,"STOP: %s\n",e.what()); }
            catch(...) { std::fputs("STOP: unhandled host error\n",stderr); }
            std::printf("Diagnostic finished: %d\n",result); std::fflush(nullptr);
            finished=true;
            dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[result,reason] {
                winrt::Windows::UI::Popups::MessageDialog dialog(
                    winrt::to_hstring("Resultado: "+std::to_string(result)+(reason.empty() ? "" : "\n"+reason)+
                    ". Pegue LocalState/loader.log e errors.log no Device Portal. O log indica a etapa que passou ou o motivo da parada; resultado 0 sozinho nao confirma jogo funcionando."),
                    L"Uncharted UWP Diagnostic");
                dialog.ShowAsync().Completed([](auto const&,auto) {
                    winrt::Windows::ApplicationModel::Core::CoreApplication::Exit();
                });
            });
        }).detach();
        std::thread([] {
            try { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
            catch(...) { std::fputs("UWP monitor apartment initialization unavailable\n",stderr); }
            ULONGLONG last=0,last_cpu_wall=0;unsigned long long last_cpu=0;
            while(!finished&&!closed) {
                const auto now=GetTickCount64();
                if(UnchartedBootTrace::enabled.load() && now-last>=5000) {
                    UnchartedBootTrace::Snapshot();
                    runtime_file_profile_report();
                    uncharted_wait_snapshot(stdout);
#ifdef UNCHARTED_GUEST_INPUT
                    runtime_pad_boot_snapshot(stdout);
#endif
                    try {
                        std::printf("UWP boot memory: %llu / %llu MiB\n",uncharted_uwp_app_memory_usage()>>20,uncharted_uwp_app_memory_limit()>>20);
                    } catch(...) { std::puts("UWP boot memory: query unavailable"); }
                    FILETIME created{},exited{},kernel{},user{};
                    if(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)) {
                        const unsigned long long cpu=(((static_cast<unsigned long long>(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime)+
                            ((static_cast<unsigned long long>(user.dwHighDateTime)<<32)|user.dwLowDateTime))/10000;
                        if(last_cpu_wall) std::printf("UWP boot CPU: process_cpu_delta=%llu ms wall_delta=%llu ms\n",cpu-last_cpu,now-last_cpu_wall);
                        last_cpu=cpu;last_cpu_wall=now;
                    }
                    last=now;
                }
                const auto limit=deadline.load();
                if(limit&&GetTickCount64()>limit) {
                    // Flush the termination reason to both streams before any
                    // detailed snapshot; GPU workers may fail concurrently.
                    std::fprintf(stdout,"STOP: diagnostic phase watchdog phase=%s; terminating with exit code 124\n",phase.load());
                    std::fflush(stdout);
                    std::fprintf(stderr,"STOP: diagnostic phase watchdog phase=%s; terminating with exit code 124\n",phase.load());
                    std::fflush(stderr);
                    UnchartedBootTrace::Snapshot();
                    runtime_file_profile_report();
                    uncharted_wait_snapshot(stdout);
#ifdef UNCHARTED_GUEST_INPUT
                    runtime_pad_boot_snapshot(stdout);
#endif
                    std::fflush(nullptr); TerminateProcess(GetCurrentProcess(),124);
                }
                Sleep(100);
            }
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
