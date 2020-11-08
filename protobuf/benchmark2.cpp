#include <benchmark/benchmark.h>

#include <container/ListArray.hpp>
#include <Protobuf.hpp>
#include "tutorial.hpp"
#include "tutorial.pb.h"

constexpr unsigned P_COUNT = 1024 * 1024;

const std::string gBuf("\x0a\x05\x4b\x65\x76\x69\x6e\x10\x84\x86\x88\x08\x1a\x0b\x66\x6f\x6f\x40\x62\x61\x72\x2e\x63\x6f\x6d\x22\x0f\x0a\x0b\x2b\x31\x32\x33\x34\x35\x36\x37\x38\x39\x30\x10\x00\x22\x0f\x0a\x0b\x2b\x30\x39\x38\x37\x36\x35\x34\x33\x32\x31\x10\x01", 59); // NEW, with 2 phones

void BM_Google(benchmark::State& state)
{
    for (auto _ : state)
    {
        Container::ListArray<tutorial::Person> sData;
        for (unsigned i = 0; i < state.range(0); i++)
        {
            sData.push_back(tutorial::Person{});
            sData.back().ParseFromString(gBuf);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Google)->Arg(P_COUNT)->Threads(1)->Threads(4)->UseRealTime()->Unit(benchmark::kMillisecond);

void BM_Arena(benchmark::State& state)
{
    thread_local google::protobuf::Arena sArena;
    for (auto _ : state)
    {
        Container::ListArray<tutorial::Person*> sData;
        for (unsigned i = 0; i < state.range(0); i++)
        {
            auto sMsg = google::protobuf::Arena::CreateMessage<tutorial::Person>(&sArena);
            sData.push_back(sMsg);
            sData.back()->ParseFromString(gBuf);
        }
        sData.clear();
        sArena.Reset();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Arena)->Arg(P_COUNT)->Threads(1)->Threads(4)->UseRealTime()->Unit(benchmark::kMillisecond);

void BM_PMR(benchmark::State& state)
{
    thread_local std::pmr::monotonic_buffer_resource sPool(1024*1024);
    thread_local std::pmr::polymorphic_allocator<pmr_tutorial::Person> sAlloc(&sPool);
    for (auto _ : state)
    {
        Container::ListArray<pmr_tutorial::Person*> sData;
        for (unsigned i = 0; i < state.range(0); i++)
        {
            auto sPtr = sAlloc.allocate(1);
            sAlloc.construct(sPtr, &sPool);
            sData.push_back(sPtr);
            sData.back()->ParseFromString(gBuf);
        }
        sData.clear();
        sPool.release();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PMR)->Arg(P_COUNT)->Threads(1)->Threads(4)->UseRealTime()->Unit(benchmark::kMillisecond);

void BM_PMR_View(benchmark::State& state)
{
    thread_local std::pmr::monotonic_buffer_resource sPool(1024*1024);
    thread_local std::pmr::polymorphic_allocator<pmr_tutorial::PersonView> sAlloc(&sPool);
    for (auto _ : state)
    {
        Container::ListArray<pmr_tutorial::PersonView*> sData;
        for (unsigned i = 0; i < state.range(0); i++)
        {
            auto sPtr = sAlloc.allocate(1);
            sAlloc.construct(sPtr, &sPool);
            sData.push_back(sPtr);
            sData.back()->ParseFromString(gBuf);
        }
        sData.clear();
        sPool.release();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PMR_View)->Arg(P_COUNT)->Threads(1)->Threads(4)->UseRealTime()->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();