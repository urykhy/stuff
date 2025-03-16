#include <benchmark/benchmark.h>

#include "Coro.hpp"

#include <time/Meter.hpp>
#include <prometheus/Histogramm.hpp>

using namespace std::chrono_literals;

static void BM_Produce(benchmark::State& state)
{
    auto sProducer = std::make_shared<Kafka::Coro::Producer>(Kafka::Options{{"bootstrap.servers", "broker-1.kafka"}, {"client.id", "bench/producer"}}, "t_benchmark");

    boost::asio::io_service sAsio;
    bool                    sExit    = false;
    uint32_t                sCount   = 0;
    uint32_t                sRunning = 0;
    Prometheus::Histogramm  sLatency;

    boost::asio::co_spawn(
        sAsio,
        [&]() -> boost::asio::awaitable<void> {
            boost::asio::steady_timer sTimer(sAsio);
            for (auto _ : state) {
                sTimer.expires_from_now(1ms);
                co_await sTimer.async_wait(boost::asio::use_awaitable);
            }
            sExit = true;
        },
        boost::asio::detached);

    for (int i = 0; i < state.range(0); i++) {
        boost::asio::co_spawn(
            sAsio,
            [&]() -> boost::asio::awaitable<void> {
                sRunning++;
                if (sRunning == 1) {
                    co_await sProducer->start();
                }
                while (!sExit) {
                    Time::Meter sMeter;
                    co_await sProducer->push(RdKafka::Topic::PARTITION_UA, {}, "bench-value");
                    sLatency.tick(sMeter.get().to_ms());
                    sCount++;
                }
                sRunning--;
                if (!sRunning) {
                    sProducer->stop();
                }
            },
            boost::asio::detached);
    }

    sAsio.run();
    state.SetItemsProcessed(sCount);
    constexpr std::array<double, 4> sProb{0.5, 0.9, 0.99, 1.0};
    const auto sResult = sLatency.quantile(sProb);

    state.counters["latency(q0.50)"] = sResult[0];
    state.counters["latency(q0.90)"] = sResult[1];
    state.counters["latency(q0.99)"] = sResult[2];
    state.counters["latency(q1.00)"] = sResult[3];
}
BENCHMARK(BM_Produce)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Arg(2500)->Arg(5000)->Arg(7500)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();