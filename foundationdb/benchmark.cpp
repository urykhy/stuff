#include <benchmark/benchmark.h>

#include "Client.hpp"

#include <unsorted/Benchmark.hpp>

static void BM_Get(benchmark::State& state)
{
    FDB::Client sClient;

    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            FDB::Transaction sTxn(sClient);
            sTxn.Set("foo", "bar");
            co_await sTxn.CoCommit();
        },
        [&]() -> boost::asio::awaitable<void> {
            FDB::Transaction sTxn(sClient);
            auto             sFuture = sTxn.Get("foo");
            co_await sFuture.CoWait();
            benchmark::DoNotOptimize(sFuture.Get());
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_Get)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Arg(1000)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();