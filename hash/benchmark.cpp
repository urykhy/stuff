#include <benchmark/benchmark.h>

// clang-format off
#include <string.h>
#include "gperf.hpp"
//  clang-format on

static void BM_GPERF(benchmark::State& state)
{
    for (auto _ : state) {
        benchmark::DoNotOptimize(test_hash_gperf::in_word_set("name", 4));
        benchmark::DoNotOptimize(test_hash_gperf::in_word_set("enum", 4));
    }
}
BENCHMARK(BM_GPERF);

BENCHMARK_MAIN();
