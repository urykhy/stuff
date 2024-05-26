#include <benchmark/benchmark.h>
#include <sys/time.h>

#include <mutex>
#include <shared_mutex>

#include "Spinlock.hpp"

template <class T>
static void BM_Test(benchmark::State& state)
{
    uint64_t sCounter = 0;
    static T sMutex;
    for (auto _ : state) {
        sMutex.lock();
        benchmark::DoNotOptimize(sCounter++);
        sMutex.unlock();
    }
}

template <class T>
static void BM_STest(benchmark::State& state)
{
    uint64_t sCounter = 0;
    static T sMutex;
    for (auto _ : state) {
        sMutex.lock_shared();
        benchmark::DoNotOptimize(sCounter++);
        sMutex.unlock_shared();
    }
}

BENCHMARK_TEMPLATE(BM_Test, std::mutex)->Threads(1)->Threads(4)->Threads(12);
BENCHMARK_TEMPLATE(BM_Test, std::shared_mutex)->Threads(1)->Threads(4)->Threads(12);
BENCHMARK_TEMPLATE(BM_STest, std::shared_mutex)->Threads(1)->Threads(4)->Threads(12);
BENCHMARK_TEMPLATE(BM_Test, std::timed_mutex)->Threads(1)->Threads(4)->Threads(12);
BENCHMARK_TEMPLATE(BM_Test, Threads::Spinlock)->Threads(1)->Threads(4)->Threads(12);
BENCHMARK_TEMPLATE(BM_Test, Threads::Adaptive)->Threads(1)->Threads(4)->Threads(12);

/*
some results for i5-8400 (6 core, no HT)

Benchmark                                             Time           CPU Iterations
BM_Test<std::mutex>/threads:1_median                 14 ns         14 ns   50457897
BM_Test<std::mutex>/threads:4_median                 40 ns        154 ns    4553604
BM_Test<std::mutex>/threads:12_median                50 ns        289 ns    2553756
BM_Test<Threads::Spinlock>/threads:1_median           7 ns          7 ns  102048184
BM_Test<Threads::Spinlock>/threads:4_median           9 ns         37 ns   18987900
BM_Test<Threads::Spinlock>/threads:12_median          8 ns         50 ns   23494932

taskset -c 1:
Benchmark                                             Time           CPU Iterations
BM_Test<std::mutex>/threads:1_median                 14 ns         14 ns   50676191
BM_Test<std::mutex>/threads:4_median                 13 ns         14 ns   50853556
BM_Test<std::mutex>/threads:12_median                13 ns         14 ns   50044776
BM_Test<Threads::Spinlock>/threads:1_median           7 ns          7 ns  103445887
BM_Test<Threads::Spinlock>/threads:4_median          14 ns         17 ns   40000000
BM_Test<Threads::Spinlock>/threads:12_median         23 ns         33 ns   23425884

Spinlock with _mm_pause() have no effect
*/

BENCHMARK_MAIN();