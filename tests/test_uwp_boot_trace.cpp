#include "boot_trace.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) {std::fprintf(stderr,"Trace FAIL %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
void trace_from_peer();
using namespace UnchartedBootTrace;
static std::string snapshot() {
    FILE* file=std::tmpfile();CHECK(file!=nullptr);Snapshot(file);std::rewind(file);
    std::string result;char line[512];while(std::fgets(line,sizeof(line),file)) result+=line;
    std::fclose(file);return result;
}
int main() {
    static_assert(std::size(names)==static_cast<unsigned>(Kind::Count));
    {Scope off(Kind::Compute);}CHECK(counters[static_cast<unsigned>(Kind::Compute)].started==0);
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
    std::printf("Boot trace contract: PASS checks=%u; cross-TU state, concurrency, scopes, saturation, snapshots\n",checks);
}
