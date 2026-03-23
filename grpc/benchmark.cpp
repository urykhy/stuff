#include <benchmark/benchmark.h>

#include <chrono>

#include "PlayGRPC.hpp"

#include <unsorted/Benchmark.hpp>

template <class H>
inline void BenchmarkTradias(benchmark::State& state, unsigned aCount, boost::asio::io_context& aContext, H aHandler)
{
    std::atomic<bool>      sExit  = false;
    std::atomic<uint32_t>  sCount = 0;
    std::atomic<uint32_t>  sError = 0;
    std::mutex             sMutex;
    Prometheus::Histogramm sLatency;

    for (unsigned i = 0; i < aCount; i++) {
        boost::asio::co_spawn(
            aContext,
            [&, aSerial = i]() -> boost::asio::awaitable<void> {
                while (!sExit) {
                    Time::Meter sMeter;
                    try {
                        co_await aHandler(aSerial);
                        std::unique_lock sLock(sMutex);
                        sLatency.tick(sMeter.get().to_ms());
                        sCount++;
                    } catch (...) {
                        sError++;
                    }
                }
            },
            boost::asio::detached);
    }

    Time::Meter sMeter;
    for (auto _ : state) {
        Threads::sleep(0.001);
    }
    sExit = true;
    aContext.stop();

    const double                    sELA = sMeter.get().to_double();
    constexpr std::array<double, 4> sProb{0.5, 0.99, 1.0};
    const auto                      sResult = sLatency.quantile(sProb);
    state.counters["rps"]                   = sCount / sELA;
    state.counters["err"]                   = sError / sELA;
    state.counters["lat(0.50)"]             = sResult[0];
    state.counters["lat(0.99)"]             = sResult[1];
    state.counters["lat(1.00)"]             = sResult[2];
}

static void BM_Produce(benchmark::State& state)
{
    using namespace std::chrono_literals;

    const std::string       sAddr = "127.0.0.1:56780";
    PlayGRPC::TradiasServer sServer(2 /* threads */);
    PlayGRPC::TradiasClient sClient(sAddr, 2 /* threads */);

    sServer.Start(sAddr);
    sClient.Start();
    std::this_thread::sleep_for(10ms);
    boost::asio::io_context sContext;
    auto                    sGuard = boost::asio::make_work_guard(sContext);
    Threads::Group          sThreads;
    sThreads.start([&]() { prctl(PR_SET_NAME, "asio:context"); sContext.run(); }, 2 /* threads */);
    sThreads.at_stop([&]() { sGuard.reset(); });

    BenchmarkTradias(
        state,
        state.range(0),
        sContext,
        [&](auto i) -> boost::asio::awaitable<void> {
            co_await sClient.Ping(i);
        });
}
BENCHMARK(BM_Produce)->UseRealTime()->Arg(1)->Arg(10)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
