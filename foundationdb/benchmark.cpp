#include <benchmark/benchmark.h>

#include "Client.hpp"

#include <parser/Atoi.hpp>
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
        [&](auto) -> boost::asio::awaitable<void> {
            FDB::Transaction sTxn(sClient);
            auto             sFuture = sTxn.Get("foo");
            co_await sFuture.CoWait();
            benchmark::DoNotOptimize(sFuture.Get());
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_Get)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Arg(1000)->Unit(benchmark::kMillisecond);

static void BM_Set(benchmark::State& state)
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
        [&](unsigned aSerial) -> boost::asio::awaitable<void> {
            const std::string sKey = "tmp-" + std::to_string(aSerial);
            FDB::Transaction  sTxn(sClient);
            auto              sFuture = sTxn.Get(sKey);
            co_await sFuture.CoWait();
            const auto  sResult = sFuture.Get();
            std::string sVal;
            if (sResult == std::nullopt) {
                sVal = "0";
            } else {
                sVal = std::to_string(Parser::Atoi<unsigned>(*sResult) + 1);
            }
            sTxn.Set(sKey, sVal);
            co_await sTxn.CoCommit();
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_Set)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Arg(1000)->Unit(benchmark::kMillisecond);

static void BM_M10Set(benchmark::State& state)
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
        [&](unsigned aSerial) -> boost::asio::awaitable<void> {
            const int                KEY_PER_TRANSACTION = 10;
            std::vector<FDB::Future> sFuture;
            FDB::Transaction         sTxn(sClient);
            for (int i = 0; i < KEY_PER_TRANSACTION; i++) {
                const std::string sKey = "tmp-" + std::to_string(aSerial) + "-" + std::to_string(i);
                sFuture.emplace_back(sTxn.Get(sKey));
            }
            for (int i = 0; i < KEY_PER_TRANSACTION; i++) {
                co_await sFuture[i].CoWait();
            }
            for (int i = 0; i < KEY_PER_TRANSACTION; i++) {
                const std::string sKey    = "tmp-" + std::to_string(aSerial) + "-" + std::to_string(i);
                const auto        sResult = sFuture[i].Get();
                std::string       sVal;
                if (sResult == std::nullopt) {
                    sVal = "0";
                } else {
                    sVal = std::to_string(Parser::Atoi<unsigned>(*sResult) + 1);
                }
                sTxn.Set(sKey, sVal);
            }
            co_await sTxn.CoCommit();
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_M10Set)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
