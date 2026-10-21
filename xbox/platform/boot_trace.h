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
    BridgeAcquire, BridgePresent, TimelineQuery, Count };
inline constexpr const char* names[] = {"shader", "translate", "spirv", "graphics_pipeline",
    "compute_pipeline", "driver_graphics", "driver_compute", "scheduler_submit",
    "record_sync", "copy_wait", "driver_submit", "gpu_wait", "draw_drain",
    "bridge_acquire", "bridge_present", "timeline_query"};
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
