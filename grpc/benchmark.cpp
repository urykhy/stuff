#include <benchmark/benchmark.h>

#include <chrono>

#include "PlayGRPC.hpp"

#include <unsorted/Benchmark.hpp>

template <class H>
inline void BenchmarkTradias(benchmark::State& state, unsigned aCount, agrpc::GrpcContext& aContext, H aHandler)
{
    bool                   sExit  = false;
    uint32_t               sCount = 0;
    uint32_t               sError = 0;
    Prometheus::Histogramm sLatency;

    for (unsigned i = 0; i < aCount; i++) {
        boost::asio::co_spawn(
            aContext,
            [&, aSerial = i]() -> boost::asio::awaitable<void> {
                while (!sExit) {
                    Time::Meter sMeter;
                    try {
                        co_await aHandler(aSerial);
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
    PlayGRPC::TradiasServer sServer;
    PlayGRPC::TradiasClient sClient(sAddr);

    sServer.Start(sAddr);
    sClient.Start();
    std::this_thread::sleep_for(10ms);

    BenchmarkTradias(
        state,
        state.range(0),
        sClient.Context(),
        [&](auto i) -> boost::asio::awaitable<void> {
            co_await sClient.Ping(i);
        });
}
BENCHMARK(BM_Produce)->UseRealTime()->Arg(1)->Arg(10)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
