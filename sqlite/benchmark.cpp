#include <benchmark/benchmark.h>
#include <xxhash.h>

#include <filesystem>

#include "Lite.hpp"

#include <parser/Atoi.hpp>
#include <prometheus/Histogramm.hpp>
#include <time/Meter.hpp>
#include <unsorted/Random.hpp>

static void BM_GetSet(benchmark::State& state)
{
    using namespace std::chrono_literals;

    constexpr int     PADDING_LENGTH = 1500;
    const std::string sPadding(PADDING_LENGTH, 'x');
    constexpr int     KEYS_COUNT = 1000000;

    std::filesystem::remove("__benchmark.db");
    Lite::DB sDB("__benchmark.db");
    sDB.Query("PRAGMA cache_size=-800000");
    sDB.Query("PRAGMA journal_mode=WAL", [](int aColumns, char const* const* aValues, char const* const* aNames) {});
    sDB.Query("CREATE TABLE IF NOT EXISTS store(Hash MEDIUMINT UNSIGNED NOT NULL PRIMARY KEY, Key TEXT NOT NULL, Value TEXT NOT NULL)");

    auto sHasher = [](const std::string& aKey) -> uint32_t { return XXH3_64bits(aKey.data(), aKey.size()); };

    auto sKvQueryGet = sDB.Prepare("SELECT Value FROM store WHERE Hash=? AND Key = ?");
    auto sKvGet      = [&](const std::string& aKey) {
        std::optional<std::string> sResult;
        sKvQueryGet.Assign(sHasher(aKey), aKey);
        sKvQueryGet.Use([&](int aColumns, char const* const* aValues, char const* const* aNames) mutable {
            sResult = aValues[0];
        });
        return sResult;
    };

    auto sKvQuerySet = sDB.Prepare("REPLACE INTO store (Hash, Key, Value) VALUES (?, ?, ?)");
    auto sKvSet      = [&](const std::string& aKey, const std::string& aValue) {
        sKvQuerySet.Assign(sHasher(aKey), aKey, aValue);
        sKvQuerySet.Use([&](int aColumns, char const* const* aValues, char const* const* aNames) mutable {});
    };

    auto sReadOp = [&]() {
        const std::string sKey = "tmp-" + std::to_string(Util::randomInt(KEYS_COUNT));
        sKvGet(sKey);
    };

    auto sWriteOp = [&]() {
        const std::string sKey = "tmp-" + std::to_string(Util::randomInt(KEYS_COUNT));

        std::string sVal;
        auto        sResult = sKvGet(sKey);
        if (sResult == std::nullopt) {
            sVal = "0";
        } else {
            std::string_view sTmp = *sResult;
            if (sTmp.size() > PADDING_LENGTH) {
                sTmp.remove_suffix(PADDING_LENGTH);
            }
            sVal = std::to_string(Parser::Atoi<unsigned>(sTmp) + 1);
        }
        sKvSet(sKey, sVal + sPadding);
    };

    Prometheus::Histogramm sReadLatency;
    Prometheus::Histogramm sWriteLatency;

    uint32_t    sTotal = 0;
    Time::Meter sMeter;

    for (auto _ : state) {
        if (sTotal % 2 == 0) {
            Time::Meter sMeter;
            sReadOp();
            sReadLatency.tick(sMeter.get().to_ms());
        } else {
            Time::Meter sMeter;
            sWriteOp();
            sWriteLatency.tick(sMeter.get().to_ms());
        }
        sTotal++;
    }

    constexpr std::array<double, 3> sProb{0.5, 0.99, 1.0};
    {
        auto sResult                  = sReadLatency.quantile(sProb);
        state.counters["r:lat(0.50)"] = sResult[0];
        state.counters["r:lat(0.99)"] = sResult[1];
        state.counters["r:lat(1.00)"] = sResult[2];
        sResult                       = sWriteLatency.quantile(sProb);
        state.counters["w:lat(0.50)"] = sResult[0];
        state.counters["w:lat(0.99)"] = sResult[1];
        state.counters["w:lat(1.00)"] = sResult[2];
        state.counters["rps"]         = sTotal / sMeter.get().to_double();
    }
}
BENCHMARK(BM_GetSet)->UseRealTime()->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
