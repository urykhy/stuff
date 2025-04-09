#include <benchmark/benchmark.h>

#include <chrono>
#include <thread>

#include "Client.hpp"
#include "Server.hpp"

#include <unsorted/Benchmark.hpp>
#include <unsorted/Raii.hpp>

static void BM_Get(benchmark::State& state)
{
    using namespace AsioHttp;

    ba::io_service sAsio;
    auto           sHttpServer = createServer({});
    auto           sKvServer   = std::make_shared<KV::Server>();
    sKvServer->Configure(sHttpServer);
    auto sClient = std::make_shared<KV::BatchClient>();

    ba::co_spawn(sAsio, [&]() -> ba::awaitable<void> { co_return co_await sHttpServer->run(); }, ba::detached);
    std::thread sHttpThread([&]() { sAsio.run(); });
    Util::Raii  sCleanup([&]() { sAsio.stop(); sHttpThread.join(); });

    Time::Meter sMeter;
    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            KV::Request sRequest{.key = "foo", .value = "bar"};
            auto        sResponse = co_await sClient->Call(sRequest);
            co_return;
        },
        [&](auto) -> boost::asio::awaitable<void> {
            KV::Request sRequest{.key = "foo"};
            auto        sResponse = co_await sClient->Call(sRequest);
            co_return;
        },
        // reset sClient here, do not leak refs to internal asio ..
        [&]() -> boost::asio::awaitable<void> {
            state.counters["http rps"] = sClient->HttpRequests() / sMeter.get().to_double();
            sClient.reset();
            co_return;
        });
}
BENCHMARK(BM_Get)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Arg(1000)->Arg(2000)->Arg(3000)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();