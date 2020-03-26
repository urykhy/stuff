#include <benchmark/benchmark.h>
#include "Stat.hpp"

static void BM_Tick(benchmark::State& state)
{
    Stat::Counter sCounter("not used","as well");
    for (auto _ : state)
        sCounter.tick();
}
BENCHMARK(BM_Tick)->Threads(1)->Threads(4)->UseRealTime();

static void BM_Time(benchmark::State& state)
{
    Stat::Time sTime("not used","as well");
    for (auto _ : state)
        sTime.account(1);
}
BENCHMARK(BM_Time)->Threads(1)->Threads(4)->UseRealTime();

BENCHMARK_MAIN();