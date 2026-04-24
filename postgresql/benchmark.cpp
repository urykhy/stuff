#include <benchmark/benchmark.h>

#include "Client.hpp"

#include <container/Pool.hpp>
#include <unsorted/Benchmark.hpp>
#include <unsorted/Env.hpp>

const std::string sRemote = fmt::format("host={} user={} password={}", Util::getEnv("PQ_HOST"), Util::getEnv("PQ_USER"), Util::getEnv("PQ_PASS"));

static void BM_Get(benchmark::State& state)
{
    PQ::Params sParams{.single_row_mode = true};
    using ClientPtr = std::shared_ptr<PQ::Client>;
    Container::Pool<ClientPtr> sPool;

    auto Get = [&]() -> boost::asio::awaitable<ClientPtr> {
        auto sOpt = sPool.get();
        if (!sOpt) {
            auto sPtr = std::make_shared<PQ::Client>(sParams);
            co_await sPtr->Connect(sRemote);
            co_return sPtr;
        }
        co_return sOpt.value();
    };

    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            auto sClient = co_await Get();
            co_await sClient->Query("INSERT into keyvalue(key, value) VALUES ('foo', 'bar') ON CONFLICT (key) DO UPDATE SET value=EXCLUDED.value");
            sPool.insert(sClient);
        },
        [&](auto) -> boost::asio::awaitable<void> {
            auto sClient = co_await Get();
            co_await sClient->Query("SELECT value FROM keyvalue WHERE key='foo'", [](auto&& sRow) {
                benchmark::DoNotOptimize(sRow.Get(0));
            });
            sPool.insert(sClient);
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_Get)->UseRealTime()->Arg(1)->Arg(10)->Arg(100)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
