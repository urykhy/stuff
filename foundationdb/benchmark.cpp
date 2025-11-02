#include <benchmark/benchmark.h>

#include <chrono>

#include "Client.hpp"
#include "Overlap.hpp"

#include <parser/Atoi.hpp>
#include <unsorted/Benchmark.hpp>
#include <unsorted/Random.hpp>

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
BENCHMARK(BM_Get)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Unit(benchmark::kMillisecond);

static void BM_GetBy100(benchmark::State& state)
{
    FDB::Client sClient;
    unsigned    COUNT = 100;
    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            FDB::Transaction sTxn(sClient);
            for (unsigned i = 0; i < COUNT; i++) {
                sTxn.Set("foo-" + std::to_string(i), "bar");
            }
            co_await sTxn.CoCommit();
        },
        [&](auto) -> boost::asio::awaitable<void> {
            FDB::Transaction       sTxn(sClient);
            std::list<FDB::Future> sFuture;
            for (unsigned i = 0; i < COUNT; i++) {
                sFuture.push_back(sTxn.Get("foo-" + std::to_string(i)));
            }
            for (auto& x : sFuture) {
                co_await x.CoWait();
                benchmark::DoNotOptimize(x.Get());
            }
        },
        [&]() -> boost::asio::awaitable<void> { co_return; });
}
BENCHMARK(BM_GetBy100)->UseRealTime()->Arg(1)->Arg(100)->Unit(benchmark::kMillisecond);

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
BENCHMARK(BM_Set)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Unit(benchmark::kMillisecond);

static void BM_GetSet(benchmark::State& state)
{
    const bool   sOverlapMode = state.range(0);
    const bool   sGRV         = state.range(1);
    FDB::Client  sClient(sGRV);
    FDB::Overlap sOverlap(sClient);

    constexpr int     PADDING_LENGTH = 1500;
    const std::string sPadding(PADDING_LENGTH, 'x');
    constexpr int     KEYS_COUNT = 1000000;

    auto sReadOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "tmp-" + std::to_string(Util::randomInt(KEYS_COUNT));
        FDB::Transaction  sTxn(sClient);
        if (sOverlapMode) {
            if (auto sVersion = sOverlap.GetVersionTimestamp(); sVersion > 0) {
                sTxn.SetVersionTimestamp(sVersion);
            }
        }
        auto sFuture = sTxn.Get(sKey);
        co_await sFuture.CoWait();
        sFuture.Get();
    };

    auto sWriteOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "tmp-" + std::to_string(Util::randomInt(KEYS_COUNT));
        FDB::Transaction  sTxn(sClient);

        if (sOverlapMode) {
            if (auto sVersion = sOverlap.GetVersionTimestamp(); sVersion > 0) {
                sTxn.SetVersionTimestamp(sVersion);
            }
        }
        auto        sFuture = sTxn.Get(sKey, true /* snapshot read */); // avoid transaction conflicts
        std::string sVal;
        co_await sFuture.CoWait();
        auto sResult = sFuture.Get();
        if (sResult == std::nullopt) {
            sVal = "0";
        } else {
            std::string_view sTmp = *sResult;
            if (sTmp.size() > PADDING_LENGTH) {
                sTmp.remove_suffix(PADDING_LENGTH);
            }
            sVal = std::to_string(Parser::Atoi<unsigned>(sTmp) + 1);
        }
        sTxn.Set(sKey, sVal + sPadding);
        co_await sTxn.CoCommit();
    };

    auto sInit = [&]() -> boost::asio::awaitable<void> {
        if (sOverlapMode) {
            co_await sOverlap.Start();
        }
    };

    auto sFini = [&]() -> boost::asio::awaitable<void> {
        if (sOverlapMode) {
            sOverlap.Stop();
        }
        co_return;
    };

    const Benchmark::GetSetConfig sBenchConfig{
        .COUNT         = 20,   // num of coroutines
        .MAX_READ_RPS  = 1000, // rps per coro
        .MAX_WRITE_RPS = 1000, // rps per coro
    };

    Benchmark::GetSet(state, sBenchConfig, sInit, sReadOp, sWriteOp, sFini);
}
BENCHMARK(BM_GetSet)->UseRealTime()->Args({0, 0})->Args({0, 1})->Args({1, 0})->Unit(benchmark::kMillisecond);

static void BM_MultiSet(benchmark::State& state)
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
BENCHMARK(BM_MultiSet)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
