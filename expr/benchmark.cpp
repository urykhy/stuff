#include <benchmark/benchmark.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <tsl/hopscotch_map.h>
#pragma GCC diagnostic pop

#include "Expr.hpp"

static void BM_Interning(benchmark::State& aState)
{
    Expr::UserData    sData;
    const std::string sJson = R"({"param-A1": 25})";
    glz::json_t       sParsed;
    if (glz::read_json(sParsed, sJson)) {
        throw std::invalid_argument("fail to parse json");
    }
    sData.m_Params["source-A"] = sParsed;
    sData.Prepare();

    // call with tcc
    const std::string sCode = R"(
    extern int getIntValue(void*, int, const char*);
    int calcSegment(void* data) {
        const int x = getIntValue(data, 0, "source-A.param-A1");
        return (x > 5 && x < 20) || (x > 20 && x < 30);
    }
    )";
    Expr::Compiler    sCompiler;
    sCompiler.Compile(sCode, "calcSegment");

    for (auto _ : aState) {
        benchmark::DoNotOptimize(sCompiler.Call(sData));
    }
}
BENCHMARK(BM_Interning);

static void BM_OpCache(benchmark::State& aState)
{
    Expr::UserData    sData;
    const std::string sJson = R"({"param-A1": 25})";
    glz::json_t       sParsed;
    if (glz::read_json(sParsed, sJson)) {
        throw std::invalid_argument("fail to parse json");
    }
    sData.m_Params["source-A"] = sParsed;
    sData.Prepare();

    // call with tcc
    const std::string sCode = R"(
    extern int getIntValue(void*, int, const char*);
    extern int isCached(void*, int);
    extern int readCache(void*, int);
    extern int storeCache(void*, int, int);
    extern int delay();
    int calcSegment(void* data) {
        if (isCached(data, 0) && readCache(data, 0)) { return 1; }
        if (isCached(data, 1) && readCache(data, 1)) { return 1; }
        const int x = getIntValue(data, 0, "source-A.param-A1");
        // return (x > 5 && x < 20 && delay()) || (x > 20 && x < 30 && delay());
        return (isCached(data, 0) ? readCache(data, 0) : storeCache(data, 0, (x > 5 && x < 20 && delay())))
            || (isCached(data, 1) ? readCache(data, 1) : storeCache(data, 1, (x > 20 && x < 30 && delay())));
    }
    )";
    Expr::Compiler    sCompiler;
    sCompiler.Compile(sCode, "calcSegment");

    for (auto _ : aState) {
        benchmark::DoNotOptimize(sCompiler.Call(sData));
    }
}
BENCHMARK(BM_OpCache);

static void BM_SCache(benchmark::State& aState)
{
    Expr::UserData    sData;
    const std::string sJson = R"({"param-A1": 25})";
    glz::json_t       sParsed;
    if (glz::read_json(sParsed, sJson)) {
        throw std::invalid_argument("fail to parse json");
    }
    sData.m_Params["source-A"] = sParsed;
    sData.Prepare();

    tsl::hopscotch_map<std::string, int> sCache;

    // call with tcc
    const std::string sCode = R"(
    extern int getIntValue(void*, int, const char*);
    extern int delay();
    int calcSegment(void* data) {
        const int x = getIntValue(data, 0, "source-A.param-A1");
        return (x > 5 && x < 20 && delay()) || (x > 20 && x < 30 && delay());
    }
    )";
    Expr::Compiler    sCompiler;
    sCompiler.Compile(sCode, "calcSegment");

    for (auto _ : aState) {
        std::string sArgs = "source-A:" + std::to_string(Expr::Compiler::tccGetInt(&sData, 0, "source-A.param-A1"));
        if (auto sIt = sCache.find(sArgs); sIt == sCache.end()) {
            int sRes      = sCompiler.Call(sData);
            sCache[sArgs] = sRes;
        } else {
            int sVal = sIt->second;
            benchmark::DoNotOptimize(sVal);
        }
        // benchmark::DoNotOptimize(sArgs);
    }
}
BENCHMARK(BM_SCache);

BENCHMARK_MAIN();
