#include <benchmark/benchmark.h>
#include <sys/time.h>

#include <mutex>
#include <shared_mutex>

#include "Spinlock.hpp"

using namespace std::chrono_literals;

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

static void BM_UpdateAtomicMap(benchmark::State& state)
{
    using T = std::map<std::string, int>;
    static Threads::AtomicSharedPtr<T> sPtr;
    uint64_t                           sCounter = 0;
    uint64_t                           sTmp     = 0;

    if (state.thread_index() == 0) {
        sPtr.Update([](auto x) { x = std::make_shared<T>(T{{"foo", 1}, {"bar", 2}, {"no one", 3}}); });
    }

    for (auto _ : state) {
        if (sCounter % 100) {
            benchmark::DoNotOptimize(sTmp += sPtr.Read()->operator[]("foo"));
            std::this_thread::sleep_for(10us);
        } else {
            sPtr.Update([](auto x) { x->operator[]("foo")++; std::this_thread::sleep_for(100us); });
        }
        sCounter++;
    }
}
BENCHMARK(BM_UpdateAtomicMap)->Threads(1)->Threads(4)->Threads(12)->UseRealTime()->Unit(benchmark::kMicrosecond);

static void BM_UpdateSMutexMap(benchmark::State& state)
{
    using T = std::map<std::string, int>;
    static std::shared_mutex sMutex;
    static T                 sMap;
    uint64_t                 sCounter = 0;
    uint64_t                 sTmp     = 0;

    if (state.thread_index() == 0) {
        sMap     = T{{"foo", 1}, {"bar", 2}, {"no one", 3}};
    };

    for (auto _ : state) {
        if (sCounter % 100) {
            std::shared_lock sLock(sMutex);
            benchmark::DoNotOptimize(sTmp += sMap["foo"]);
            std::this_thread::sleep_for(10us);
        } else {
            std::unique_lock sLock(sMutex);
            sMap["foo"]++;
            std::this_thread::sleep_for(100us);
        }
        sCounter++;
    }
}
BENCHMARK(BM_UpdateSMutexMap)->Threads(1)->Threads(4)->Threads(12)->UseRealTime()->Unit(benchmark::kMicrosecond);

/*
Benchmark                                        Time             CPU   Iterations
BM_UpdateAtomicMap/real_time/threads:1        63.5 us         2.04 us        11077
BM_UpdateAtomicMap/real_time/threads:4        15.8 us         2.20 us        44768
BM_UpdateAtomicMap/real_time/threads:12       5.72 us         5.88 us       115680
BM_UpdateSMutexMap/real_time/threads:1        63.8 us         2.29 us        11002
BM_UpdateSMutexMap/real_time/threads:4        32.0 us         2.34 us        21824
BM_UpdateSMutexMap/real_time/threads:12       31.5 us         2.35 us        22716
*/

BENCHMARK_MAIN();