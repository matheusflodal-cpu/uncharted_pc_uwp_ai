#include "boot_trace.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <coroutine>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) {std::fprintf(stderr,"Trace FAIL %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
void trace_from_peer();
using namespace UnchartedBootTrace;
struct SuspendedWait {
    struct promise_type {
        SuspendedWait get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
    ~SuspendedWait() { handle.destroy(); }
};
static SuspendedWait suspended_wait() {
    QueueWait wait(3,1,0x3000,7,0xff,3);
    wait.Poll([] { return 5; });
    co_await std::suspend_always{};
}
static std::string snapshot() {
    FILE* file=std::tmpfile();CHECK(file!=nullptr);Snapshot(file);std::rewind(file);
    std::string result;char line[512];while(std::fgets(line,sizeof(line),file)) result+=line;
    std::fclose(file);return result;
}
int main() {
    static_assert(std::size(names)==static_cast<unsigned>(Kind::Count));
    {Scope off(Kind::Compute);}CHECK(counters[static_cast<unsigned>(Kind::Compute)].started==0);
    unsigned samples=0;
    {QueueWait off(0,1,0);off.Poll([&] { ++samples;return 0; });}
    NoteQueue(0,QueueEvent::Submit);NotePacket(0,2,3);
    CHECK(samples==0&&pm4[0].submitted==0&&pm4[0].packets==0);
    Enable();trace_from_peer();CHECK(counters[static_cast<unsigned>(Kind::Compute)].ended==1);
    {Scope held(Kind::GpuWait,99,0x1234);auto s=snapshot();CHECK(s.find("pending gpu_wait id=0x63 owner=0x1234")!=std::string::npos);}
    CHECK(counters[static_cast<unsigned>(Kind::GpuWait)].active==0);
    try {Scope failed(Kind::Shader,9);throw 1;}catch(int){}
    CHECK(counters[static_cast<unsigned>(Kind::Shader)].failed==1);
    std::vector<std::thread> workers;
    for(unsigned t=0;t<8;++t) workers.emplace_back([t]{for(unsigned i=0;i<100;++i) {Scope s(Kind::Translate,i,t);Count(draws);}});
    for(auto& t:workers)t.join();
    const auto& c=counters[static_cast<unsigned>(Kind::Translate)];
    CHECK(c.started==800);CHECK(c.ended==800);CHECK(c.active==0);CHECK(draws==800);
    std::vector<std::unique_ptr<Scope>> held;
    for(unsigned i=0;i<40;++i)held.emplace_back(std::make_unique<Scope>(Kind::CopyWait,i));
    CHECK(untracked==8);CHECK(counters[static_cast<unsigned>(Kind::CopyWait)].active==40);
    CHECK(snapshot().find("untracked_scopes=8")!=std::string::npos);
    held.clear();CHECK(counters[static_cast<unsigned>(Kind::CopyWait)].active==0);
    ObserveTimeline(0x1234,19,17);ObserveTimeline(0x1234,22,21);
    CHECK(snapshot().find("cached_timeline owner=0x1234 cpu_next=22 gpu_known=21")!=std::string::npos);
    NoteQueue(0,QueueEvent::Submit);NoteQueue(0,QueueEvent::Resume);
    NotePacket(0,0x3c,0x1000);
    {
        QueueWait graphics(0,1,0x2000,7,0xff,3),compute(1,2,0x123);
        for(unsigned i=0;i<100;++i) graphics.Poll([&] { ++samples;return 5; });
        compute.Poll([] { return 9; });
        CHECK(samples==1);CHECK(pm4[0].waiting==1&&pm4[1].waiting==2);
        auto s=snapshot();CHECK(s.find("pm4_queue=0 submitted=1 resumed=1 completed=0 packets=1 last_opcode=0x3c packet=0x1000")!=std::string::npos);
        CHECK(s.find("pm4_wait reason=1 address_or_reg=0x2000 cached_value=0x5 ref=0x7 mask=0xff function=3")!=std::string::npos);
    }
    CHECK(pm4[0].waiting==0&&pm4[1].waiting==0);
    NoteQueue(0,QueueEvent::Complete);CHECK(pm4[0].completed==1);
    {QueueWait invalid(64,1,0);invalid.Poll([&] { ++samples;return 0; });}
    NoteQueue(64,QueueEvent::Submit);NotePacket(64,2,3);CHECK(samples==1);
    {auto task=suspended_wait();CHECK(pm4[3].waiting==0);task.handle.resume();
        CHECK(pm4[3].waiting==1&&pm4[3].value==5&&!task.handle.done());
        task.handle.resume();CHECK(pm4[3].waiting==0&&task.handle.done());}
    std::printf("Boot trace contract: PASS checks=%u; cross-TU state, concurrency, scopes, saturation, snapshots, PM4 queues and suspended waits\n",checks);
}
