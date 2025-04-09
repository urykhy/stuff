#include <benchmark/benchmark.h>

#include "Coro.hpp"

#include <unsorted/Benchmark.hpp>

static void BM_Produce(benchmark::State& state)
{
    auto sProducer = std::make_shared<Kafka::Coro::Producer>(Kafka::Options{{"bootstrap.servers", "broker-1.kafka"}, {"client.id", "bench/producer"}}, "t_benchmark");

    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            co_await sProducer->start();
        },
        [&](auto) -> boost::asio::awaitable<void> {
            co_await sProducer->push(RdKafka::Topic::PARTITION_UA, {}, "bench-value");
        },
        [&]() -> boost::asio::awaitable<void> {
            sProducer->stop();
            co_return;
        });
}
BENCHMARK(BM_Produce)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Arg(2500)->Arg(5000)->Arg(7500)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();