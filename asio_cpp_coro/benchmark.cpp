#include <benchmark/benchmark.h>

#include "lib/API.hpp"
#include "lib/Asio.hpp"

#include <unsorted/Benchmark.hpp>

static void BM_Http(benchmark::State& state)
{
    auto sServer = AsioHttp::createServer({});
    sServer->addHandler("/test", [](AsioHttp::BeastRequest&& aRequest) -> boost::asio::awaitable<AsioHttp::BeastResponse> {
        AsioHttp::BeastResponse sResponse;
        sResponse.result(AsioHttp::bb::http::status::ok);
        sResponse.body() = "body";
        co_return sResponse;
    });
    auto sClient = AsioHttp::createClient({.alive = true}); // default port 3080

    Benchmark::Coro(
        state,
        state.range(0),
        [&]() -> boost::asio::awaitable<void> {
            boost::asio::co_spawn(
                co_await boost::asio::this_coro::executor,
                [&]() -> boost::asio::awaitable<void> {
                    co_return co_await sServer->run();
                },
                boost::asio::detached);
        },
        [&](auto) -> boost::asio::awaitable<void> {
            try {
                auto sRequest = AsioHttp::Request{.method = "GET", .url = "http://localhost:3080/test"};
                auto sResult  = co_await sClient->perform(std::move(sRequest));
            } catch (const std::exception& sErr) {
                // FIXME: handle failed requests
            }
            co_return;
        },
        [&]() -> boost::asio::awaitable<void> {
            sServer->stop();
            sServer.reset();
            sClient.reset();
            co_return;
        });
}
BENCHMARK(BM_Http)->UseRealTime()->Arg(1)->Arg(100)->Arg(500)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();