// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <functional>
#include <thread>

namespace UnchartedBootTrace {
using U64 = unsigned long long;
enum class Kind { Shader, Translate, Spirv, Graphics, Compute, DriverGraphics,
    DriverCompute, Submit, RecordSync, CopyWait, DriverSubmit, GpuWait, DrawDrain,
    BridgeAcquire, BridgePresent, TimelineQuery, TilingPipeline, DriverTiling,
    DrawRecord, DispatchRecord, Count };
inline constexpr const char* names[] = {"shader", "translate", "spirv", "graphics_pipeline",
    "compute_pipeline", "driver_graphics", "driver_compute", "scheduler_submit",
    "record_sync", "copy_wait", "driver_submit", "gpu_wait", "draw_drain",
    "bridge_acquire", "bridge_present", "timeline_query", "tiling_pipeline",
    "driver_tiling", "draw_record", "dispatch_record"};
struct Counter { std::atomic<U64> started{0}, ended{0}, failed{0}, active{0}; };
struct Slot {
    std::atomic<unsigned> state{0}, kind{0}; // free, initializing, active
    std::atomic<U64> id{0}, owner{0}, start{0}, thread{0};
};
inline std::array<Counter,static_cast<unsigned>(Kind::Count)> counters;
inline std::array<Slot,32> slots;
struct Timeline { std::atomic<U64> owner{0}, cpu{0}, gpu{0}; };
inline std::array<Timeline,8> timelines;
inline std::atomic<bool> enabled{false};
inline std::atomic<U64> origin{0}, untracked{0}, draws{0}, dispatches{0}, flips{0};
inline U64 Now() { return static_cast<U64>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count()); }
// Single command processor publishes samples. The monitor reads only atomics,
// never a guest address or a renderer queue/container. Values are best-effort.
struct Pm4Queue {
    std::atomic<U64> submitted{0}, resumed{0}, completed{0}, packets{0};
    std::atomic<U64> opcode{0}, packet{0}, packet_time{0};
    std::atomic<U64> waiting{0}, wait_time{0}, address{0}, value{0}, ref{0}, mask{0}, function{0}, polls{0}, sampled{0};
};
inline std::array<Pm4Queue,64> pm4;
enum class QueueEvent { Submit, Resume, Complete };
inline void NoteQueue(unsigned q,QueueEvent event) {
    if(!enabled.load(std::memory_order_relaxed)||q>=pm4.size()) return;
    auto& t=pm4[q];
    if(event==QueueEvent::Submit) ++t.submitted;
    else if(event==QueueEvent::Resume) ++t.resumed;
    else ++t.completed;
}
inline void NotePacket(unsigned q,U64 opcode,U64 address) {
    if(!enabled.load(std::memory_order_relaxed)||q>=pm4.size()) return;
    auto& t=pm4[q];t.opcode=opcode;t.packet=address;t.packet_time=Now();++t.packets;
}
// Reason: 1 memory, 2 register, 3 video label, 4 semaphore, 5 rewind.
// A coroutine keeps this object alive while suspended. Poll samples at most
// once per second; its reader executes on the command thread, not the monitor.
class QueueWait {
    Pm4Queue* trace=nullptr;U64 polls=0,next=0;
public:
    QueueWait(unsigned q,U64 reason,U64 address,U64 ref=0,U64 mask=0,U64 function=0) {
        if(!enabled.load(std::memory_order_acquire)||q>=pm4.size()) return;
        trace=&pm4[q];trace->wait_time=Now();trace->address=address;
        trace->ref=ref;trace->mask=mask;trace->function=function;
        trace->polls=0;trace->sampled=0;trace->waiting.store(reason,std::memory_order_release);
    }
    QueueWait(const QueueWait&)=delete;QueueWait& operator=(const QueueWait&)=delete;
    template<class Reader> void Poll(Reader&& read) {
        if(!trace) return;
        ++polls;
        if(polls!=1 && (polls&4095)) return;
        const U64 now=Now();if(now<next) return;
        trace->value=static_cast<U64>(read());trace->polls=polls;trace->sampled=now;next=now+1000;
    }
    ~QueueWait() { if(trace) trace->waiting.store(0,std::memory_order_release); }
};
inline void Enable() { origin=Now(); enabled.store(true,std::memory_order_release); }
inline void Count(std::atomic<U64>& counter) {
    if(enabled.load(std::memory_order_relaxed)) counter.fetch_add(1,std::memory_order_relaxed);
}
inline void ObserveTimeline(U64 owner,U64 cpu,U64 gpu) {
    if(!enabled.load(std::memory_order_relaxed)) return;
    for(auto& t:timelines) {
        U64 empty=0;
        if(t.owner.load()==owner || t.owner.compare_exchange_strong(empty,owner)) {
            t.cpu=cpu;t.gpu=gpu;return;
        }
    }
}
class Scope {
    Kind kind; U64 id,owner,start=0,serial=0; int slot=-1,exceptions=0; bool on=false;
public:
    Scope(Kind k,U64 identifier=0,U64 object=0):kind(k),id(identifier),owner(object) {
        if(!(on=enabled.load(std::memory_order_acquire))) return;
        start=Now(); exceptions=std::uncaught_exceptions();
        auto& c=counters[static_cast<unsigned>(kind)];
        serial=c.started.fetch_add(1,std::memory_order_relaxed)+1;
        c.active.fetch_add(1,std::memory_order_relaxed);
        for(unsigned i=0;i<slots.size();++i) {
            unsigned free=0;
            if(!slots[i].state.compare_exchange_strong(free,1,std::memory_order_acquire)) continue;
            slot=static_cast<int>(i);auto& s=slots[i];
            s.kind=static_cast<unsigned>(kind);s.id=id;s.owner=owner;s.start=start;
            s.thread=static_cast<U64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
            s.state.store(2,std::memory_order_release);break;
        }
        if(slot<0) untracked.fetch_add(1,std::memory_order_relaxed);
        if(serial<=8) std::printf("UWP trace begin: %s id=0x%llx owner=0x%llx t=%llu ms\n",
            names[static_cast<unsigned>(kind)],id,owner,start-origin.load());
    }
    Scope(const Scope&)=delete; Scope& operator=(const Scope&)=delete;
    ~Scope() {
        if(!on) return;
        const U64 duration=Now()-start;
        const bool failed=std::uncaught_exceptions()>exceptions;
        auto& c=counters[static_cast<unsigned>(kind)];
        c.ended.fetch_add(1,std::memory_order_relaxed);
        if(failed) c.failed.fetch_add(1,std::memory_order_relaxed);
        if(slot>=0) slots[static_cast<unsigned>(slot)].state.store(0,std::memory_order_release);
        c.active.fetch_sub(1,std::memory_order_relaxed);
        if(serial<=8 || duration>=1000 || failed)
            std::printf("UWP trace end: %s id=0x%llx owner=0x%llx duration=%llu ms outcome=%s\n",
                names[static_cast<unsigned>(kind)],id,owner,duration,failed?"exception":"returned");
    }
};
inline void Snapshot(FILE* out=stdout) {
    if(!enabled.load(std::memory_order_acquire)) return;
    const U64 now=Now();
    std::fprintf(out,"UWP boot progress: elapsed=%llu ms draws=%llu dispatches=%llu flip_requests=%llu untracked_scopes=%llu\n",
        now-origin.load(),draws.load(),dispatches.load(),flips.load(),untracked.load());
    for(unsigned i=0;i<counters.size();++i) {
        const auto& c=counters[i];
        if(c.started.load()) std::fprintf(out,"  %s started=%llu returned=%llu exceptions=%llu active=%llu\n",
            names[i],c.started.load(),c.ended.load(),c.failed.load(),c.active.load());
    }
    for(const auto& t:timelines) if(t.owner.load())
        std::fprintf(out,"  cached_timeline owner=0x%llx cpu_next=%llu gpu_known=%llu\n",
            t.owner.load(),t.cpu.load(),t.gpu.load());
    for(unsigned q=0;q<pm4.size();++q) {
        const auto& t=pm4[q];if(!t.submitted.load()&&!t.packets.load()) continue;
        const U64 at=t.packet_time.load();
        std::fprintf(out,"  pm4_queue=%u submitted=%llu resumed=%llu completed=%llu packets=%llu last_opcode=0x%llx packet=0x%llx packet_age=%llu ms\n",
            q,t.submitted.load(),t.resumed.load(),t.completed.load(),t.packets.load(),
            t.opcode.load(),t.packet.load(),at&&now>=at?now-at:0);
        if(const U64 reason=t.waiting.load(std::memory_order_acquire)) {
            const U64 began=t.wait_time.load(),sample=t.sampled.load();
            std::fprintf(out,"    pm4_wait reason=%llu address_or_reg=0x%llx cached_value=0x%llx ref=0x%llx mask=0x%llx function=%llu age=%llu ms sampled=%s sample_age=%llu ms polls_at_sample=%llu\n",
                reason,t.address.load(),t.value.load(),t.ref.load(),t.mask.load(),t.function.load(),
                now>=began?now-began:0,sample?"yes":"no",sample&&now>=sample?now-sample:0,t.polls.load());
        }
    }
    // Best-effort atomic snapshots only: never acquire a renderer/driver lock
    // or query Vulkan from the watchdog. Concurrent entries can change here.
    for(const auto& s:slots) if(s.state.load(std::memory_order_acquire)==2) {
        const unsigned k=s.kind.load();const U64 id=s.id.load(),owner=s.owner.load(),start=s.start.load(),thread=s.thread.load();
        if(s.state.load(std::memory_order_acquire)==2 && k<counters.size())
            std::fprintf(out,"  pending %s id=0x%llx owner=0x%llx age=%llu ms thread_key=0x%llx\n",
                names[k],id,owner,now>=start?now-start:0,thread);
    }
}
} // namespace UnchartedBootTrace
