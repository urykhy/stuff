#include <chrono>

#include <benchmark/Benchmark.hpp>
#include <cache/Redis.hpp>
#include <container/Pool.hpp>
#include <foundationdb/Client.hpp>
#include <parser/Atoi.hpp>
#include <postgresql/Client.hpp>
#include <unsorted/Random.hpp>

constexpr int     PADDING_LENGTH = 1000;
const std::string gPadding(PADDING_LENGTH, 'x');
constexpr int     KEYS_COUNT = 10000;

const Benchmark::GetSetConfig gBenchConfig{
    .COUNT         = 10,   // num of coroutines
    .MAX_READ_RPS  = 1000, // rps per coro (x2 from write rps)
    .MAX_WRITE_RPS = 500,  // rps per coro
};

static void BM_FDB(benchmark::State& state)
{
    FDB::Client sClient(true /* GRV cache */);

    auto sReadOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "bench-" + std::to_string(Util::randomInt(KEYS_COUNT));
        FDB::Transaction  sTxn(sClient);
        auto              sFuture = sTxn.Get(sKey);
        co_await sFuture.CoWait();
        sFuture.Get();
    };

    auto sWriteOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "bench-" + std::to_string(Util::randomInt(KEYS_COUNT));
        FDB::Transaction  sTxn(sClient);
        auto              sFuture = sTxn.Get(sKey, true /* snapshot read */); // avoid transaction conflicts
        std::string       sVal;
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
        sTxn.Set(sKey, sVal + gPadding);
        co_await sTxn.CoCommit();
    };

    Benchmark::GetSet(state, gBenchConfig, sReadOp, sWriteOp);
}
BENCHMARK(BM_FDB)->UseRealTime()->Unit(benchmark::kMillisecond);

static void BM_Redis(benchmark::State& state)
{
    Cache::Redis::Config sConfig;
    Cache::Redis::Coro   sRedis(sConfig);

    auto sWriteOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "bench-" + std::to_string(Util::randomInt(KEYS_COUNT));

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
        co_await sRedis.Set(sKey, sVal + gPadding);
    };

    auto sReadOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey = "bench-" + std::to_string(Util::randomInt(KEYS_COUNT));
        co_await sRedis.Get(sKey);
    };

    Benchmark::GetSet(state, gBenchConfig, sReadOp, sWriteOp);
}
BENCHMARK(BM_Redis)->UseRealTime()->Unit(benchmark::kMillisecond);

static void BM_Postgres(benchmark::State& state)
{
    const std::string sRemote = fmt::format("host={} user={} password={}", Util::getEnv("PQ_HOST"), Util::getEnv("PQ_USER"), Util::getEnv("PQ_PASS"));

    PQ::Params sParams{.single_row_mode = true};
    using ClientPtr = std::shared_ptr<PQ::Client>;
    Container::Pool<ClientPtr> sPool;

    auto Get = [&]() -> boost::asio::awaitable<ClientPtr> {
        auto sOpt = sPool.get();
        if (!sOpt) {
            auto sPtr = std::make_shared<PQ::Client>(sParams);
            co_await sPtr->Connect(sRemote);
            co_return sPtr;
        }
        co_return sOpt.value();
    };

    auto sWriteOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey    = "bench-" + std::to_string(Util::randomInt(KEYS_COUNT));
        auto              sClient = co_await Get();
        std::string       sVal;
        co_await sClient->Query(fmt::format("SELECT value FROM keyvalue WHERE key='{}'", sKey), [&](auto&& sRow) {
            sVal = sRow.Get(0);
        });
        if (sVal.empty()) {
            sVal = "0";
        } else {
            std::string_view sTmp = sVal;
            if (sTmp.size() > PADDING_LENGTH) {
                sTmp.remove_suffix(PADDING_LENGTH);
            }
            sVal = std::to_string(Parser::Atoi<unsigned>(sTmp) + 1);
        }
        co_await sClient->Query(fmt::format("INSERT into keyvalue(key, value) VALUES ('{}', '{}') ON CONFLICT (key) DO UPDATE SET value=EXCLUDED.value", sKey, sVal + gPadding));
        sPool.insert(sClient);
    };

    auto sReadOp = [&]() -> boost::asio::awaitable<void> {
        const std::string sKey    = "bench-" + std::to_string(Util::randomInt(KEYS_COUNT));
        auto              sClient = co_await Get();
        co_await sClient->Query(fmt::format("SELECT value FROM keyvalue WHERE key='{}'", sKey), [](auto&& sRow) {
            benchmark::DoNotOptimize(sRow.Get(0));
        });
        sPool.insert(sClient);
    };

    Benchmark::GetSet(state, gBenchConfig, sReadOp, sWriteOp);
}
BENCHMARK(BM_Postgres)->UseRealTime()->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
