#include <benchmark/benchmark.h>

#include <chrono>

#include "Redis.hpp"

#include <parser/Atoi.hpp>
#include <unsorted/Benchmark.hpp>
#include <unsorted/Random.hpp>

static void BM_GetSet(benchmark::State& state)
{
    Cache::Redis::Config sConfig;
    Cache::Redis::Coro   sRedis(sConfig);

    constexpr int     PADDING_LENGTH = 1500;
    const std::string sPadding(PADDING_LENGTH, 'x');

    constexpr int KEYS_COUNT = 1000000;
    auto          sWriteOp   = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "tmp-" + std::to_string(Util::randomInt(KEYS_COUNT));

        auto        sResult = co_await sRedis.Get(sKey);
        std::string sVal;
        if (sResult == std::nullopt) {
            sVal = "0";
        } else {
            std::string_view sTmp = *sResult;
            if (sTmp.size() > PADDING_LENGTH) {
                sTmp.remove_suffix(PADDING_LENGTH);
            }
            sVal = std::to_string(Parser::Atoi<unsigned>(sTmp) + 1);
        }
        co_await sRedis.Set(sKey, sVal + sPadding);
    };

    auto sReadOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "tmp-" + std::to_string(Util::randomInt(KEYS_COUNT));
        co_await sRedis.Get(sKey);
    };

    auto sInit = [&]() -> boost::asio::awaitable<void> { co_return; };
    auto sFini = [&]() -> boost::asio::awaitable<void> { co_return; };

    const Benchmark::GetSetConfig sBenchConfig{
        .COUNT         = 20,   // num of coroutines
        .MAX_READ_RPS  = 1000, // rps per coro
        .MAX_WRITE_RPS = 1000, // rps per coro
    };

    Benchmark::GetSet(state, sBenchConfig, sInit, sReadOp, sWriteOp, sFini);
}
BENCHMARK(BM_GetSet)->UseRealTime()->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
