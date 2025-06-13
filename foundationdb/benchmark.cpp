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
    using namespace std::chrono_literals;

    boost::asio::io_service sAsio;
    FDB::Client             sClient;
    FDB::Overlap            sOverlap(sClient);
    std::atomic_bool        sExit{false};

    constexpr int KEYS_COUNT = 50000;
    auto          sWriteOp   = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "tmp-" + std::to_string(Util::randomInt(KEYS_COUNT));
        FDB::Transaction  sTxn(sClient);
        auto              sFuture = sTxn.Get(sKey, true /* snapshot read */); // avoid transaction conflicts
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
    };

    const bool sOverlapMode = state.range(0);
    auto       sReadOp      = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "tmp-" + std::to_string(Util::randomInt(KEYS_COUNT));
        if (sOverlapMode) {
            auto sFuture = sOverlap.Get(sKey);
            co_await sFuture.CoWait();
            sFuture.Get();
        } else {
            FDB::Transaction sTxn(sClient);
            auto             sFuture = sTxn.Get(sKey);
            co_await sFuture.CoWait();
            sFuture.Get();
            co_await sTxn.CoCommit();
        }
    };

    constexpr unsigned     COUNT        = 100;  // num of coroutines
    constexpr unsigned     MAX_READ_RPS = 1000; // rps per coro
    unsigned               sReadCount   = 0;
    unsigned               sReadError   = 0;
    Prometheus::Histogramm sReadLatency;

    for (unsigned i = 0; i < COUNT; i++) {
        boost::asio::co_spawn(
            sAsio,
            [&]() -> boost::asio::awaitable<void> {
                Benchmark::RateLimit sLimit(MAX_READ_RPS);
                while (!sExit) {
                    try {
                        co_await sLimit([&]() -> boost::asio::awaitable<void> {
                            Time::Meter sMeter;
                            co_await sReadOp();
                            sReadLatency.tick(sMeter.get().to_ms());
                            sReadCount++;
                        });
                    } catch (...) {
                        sReadError++;
                    };
                }
            },
            boost::asio::detached);
    }

    unsigned               sWriteCount = 0;
    unsigned               sWriteError = 0;
    Prometheus::Histogramm sWriteLatency;
    for (unsigned i = 0; i < COUNT; i++) {
        boost::asio::co_spawn(
            sAsio,
            [&]() -> boost::asio::awaitable<void> {
                while (!sExit) {
                    Time::Meter sMeter;
                    try {
                        co_await sWriteOp();
                        sWriteLatency.tick(sMeter.get().to_ms());
                        sWriteCount++;
                    } catch (...) {
                        sWriteError++;
                    };
                }
            },
            boost::asio::detached);
    }

    boost::asio::co_spawn(
        sAsio,
        [&]() -> boost::asio::awaitable<void> {
            using namespace std::chrono_literals;
            boost::asio::steady_timer sTimer(sAsio);
            for (auto _ : state) {
                sTimer.expires_from_now(1ms);
                co_await sTimer.async_wait(boost::asio::use_awaitable);
            }
            sExit = true;
        },
        boost::asio::detached);

    Time::Meter sMeter;
    sAsio.run();
    const double sELA = sMeter.get().to_double();

    constexpr std::array<double, 4> sProb{0.5, 0.99, 1.0};
    {
        auto sResult                  = sReadLatency.quantile(sProb);
        state.counters["r:lat(0.50)"] = sResult[0];
        state.counters["r:lat(0.99)"] = sResult[1];
        state.counters["r:lat(1.00)"] = sResult[2];
        state.counters["r:rps"]       = sReadCount / sELA;
        state.counters["r:err"]       = sReadError / sELA;
        sResult                       = sWriteLatency.quantile(sProb);
        state.counters["w:lat(0.50)"] = sResult[0];
        state.counters["w:lat(0.99)"] = sResult[1];
        state.counters["w:lat(1.00)"] = sResult[2];
        state.counters["w:rps"]       = sWriteCount / sELA;
        state.counters["w:err"]       = sWriteError / sELA;
    }
}
BENCHMARK(BM_GetSet)->UseRealTime()->Arg(0)->Arg(1)->Unit(benchmark::kMillisecond);

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
