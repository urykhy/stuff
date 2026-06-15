#include <chrono>

#include "Client.hpp"

#include <benchmark/Benchmark.hpp>
#include <parser/Atoi.hpp>
#include <time/Meter.hpp>
#include <unsorted/Random.hpp>

static void BM_Get(benchmark::State& state)
{
    FDB::Client sClient(state.range(1) /* with GRV proxy */);

    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            FDB::Transaction sTxn(sClient);
            sTxn.Set("get", "bar");
            co_await sTxn.CoCommit();
        },
        [&](auto) -> boost::asio::awaitable<void> {
            FDB::Transaction sTxn(sClient);
            auto             sFuture = sTxn.Get("get");
            co_await sFuture.CoWait();
            benchmark::DoNotOptimize(sFuture.Get());
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_Get)->UseRealTime()->ArgsProduct({{1, 100}, {0, 1}})->ArgNames({"coro", "grv"})->Unit(benchmark::kMillisecond);

static void BM_GetBy100(benchmark::State& state)
{
    FDB::Client sClient(true /* grv cache */);
    unsigned    COUNT = 100;
    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            FDB::Transaction sTxn(sClient);
            for (unsigned i = 0; i < COUNT; i++) {
                sTxn.Set("get-by-100-" + std::to_string(i), "bar");
            }
            co_await sTxn.CoCommit();
        },
        [&](auto) -> boost::asio::awaitable<void> {
            FDB::Transaction       sTxn(sClient);
            std::list<FDB::Future> sFuture;
            for (unsigned i = 0; i < COUNT; i++) {
                sFuture.push_back(sTxn.Get("get-by-100-" + std::to_string(i)));
            }
            for (auto& x : sFuture) {
                co_await x.CoWait();
                benchmark::DoNotOptimize(x.Get());
            }
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_GetBy100)->UseRealTime()->Arg(1)->Arg(100)->ArgName("coro")->Unit(benchmark::kMillisecond);

static void BM_GetCachedVersion(benchmark::State& state)
{
    FDB::Client          sClient(false /* grv cache */);
    std::atomic_uint64_t sVersion{0};
    Time::time_spec      sUpdateAt = Time::time_spec::now();

    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            FDB::Transaction sTxn(sClient);
            sTxn.Set("get-vt", "bar");
            co_await sTxn.CoCommit();
        },
        [&](auto) -> boost::asio::awaitable<void> {
            auto sNow = Time::time_spec::now();
            if (sVersion == 0 or (sNow - sUpdateAt).to_ms() > 500) {
                FDB::Transaction sTxn(sClient);
                sTxn.Set("__tmp", "overlap"); // set some key, required for GetVersionTimestamp
                co_await sTxn.CoCommit();
                sVersion  = sTxn.GetVersionTimestamp();
                sUpdateAt = Time::time_spec::now();
            }

            FDB::Transaction sTxn(sClient);
            sTxn.SetVersionTimestamp(sVersion);
            auto sFuture = sTxn.Get("get-vt");
            co_await sFuture.CoWait();
            benchmark::DoNotOptimize(sFuture.Get());
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_GetCachedVersion)->Arg(1)->UseRealTime()->Unit(benchmark::kMillisecond);

static void BM_Set(benchmark::State& state)
{
    FDB::Client sClient;

    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            co_return;
        },
        [&](unsigned aSerial) -> boost::asio::awaitable<void> {
            const std::string sKey = "set-" + std::to_string(aSerial);
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
BENCHMARK(BM_Set)->UseRealTime()->Arg(1)->Arg(100)->ArgName("coro")->Unit(benchmark::kMillisecond);

static void BM_MultiSet(benchmark::State& state)
{
    FDB::Client sClient;

    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            co_return;
        },
        [&](unsigned aSerial) -> boost::asio::awaitable<void> {
            const int                KEY_PER_TRANSACTION = 10;
            std::vector<FDB::Future> sFuture;
            FDB::Transaction         sTxn(sClient);
            for (int i = 0; i < KEY_PER_TRANSACTION; i++) {
                const std::string sKey = "multi-set-" + std::to_string(aSerial) + "-" + std::to_string(i);
                sFuture.emplace_back(sTxn.Get(sKey));
            }
            for (int i = 0; i < KEY_PER_TRANSACTION; i++) {
                co_await sFuture[i].CoWait();
            }
            for (int i = 0; i < KEY_PER_TRANSACTION; i++) {
                const std::string sKey    = "multi-set-" + std::to_string(aSerial) + "-" + std::to_string(i);
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
BENCHMARK(BM_MultiSet)->UseRealTime()->Arg(1)->Arg(100)->ArgName("coro")->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
