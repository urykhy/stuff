#include <benchmark/benchmark.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include <sol/debug.hpp>
#include <sol/sol.hpp>
#pragma GCC diagnostic pop

#include "ExprTK.hpp"

#include <unsorted/Tcc.hpp>

static void
BM_Exprtk(benchmark::State& aState)
{
    Util::ExprTK sExpr;
    sExpr.m_Table.create_variable("x");
    sExpr.compile("(x > 5 and x < 20) or (x > 20 and x < 30)");

    int                      sParam  = 25;
    std::function<bool(int)> sMethod = [&sExpr](auto aParam) {
        sExpr.m_Table.get_variable("x")->ref() = aParam;
        return sExpr.eval();
    };

    for (auto _ : aState) {
        bool sResult = sMethod(sParam);
        benchmark::DoNotOptimize(sResult);
        sParam++;
        sParam %= 40;
    }
}
BENCHMARK(BM_Exprtk);

static void BM_Lua(benchmark::State& aState)
{
    sol::state sLua;
    sLua.open_libraries(sol::lib::base);

    sLua.script(R"(
        function test(x)
          return (x > 5 and x < 20) or (x > 20 and x < 30)
        end
    )");
    int           sParam  = 25;
    sol::function sMethod = sLua["test"];

    for (auto _ : aState) {
        bool sResult = sMethod(sParam);
        benchmark::DoNotOptimize(sResult);
        sParam++;
        sParam %= 40;
    }
}
BENCHMARK(BM_Lua);

static void BM_Tcc(benchmark::State& aState)
{
    const std::string  sCode = R"(
    int test1(int x) {
        return (x > 5 && x < 20) || (x > 20 && x < 30);
    }
    )";
    Util::TinyCompiler sCompiler;
    sCompiler.Compile(sCode);

    using T   = int (*)(int);
    T sMethod = (T)sCompiler.GetSymbol("test1");
    if (!sMethod) {
        throw std::invalid_argument("fail to get symbol");
    }

    int sParam = 25;
    for (auto _ : aState) {
        bool sResult = sMethod(sParam);
        benchmark::DoNotOptimize(sResult);
        sParam++;
        sParam %= 40;
    }
}
BENCHMARK(BM_Tcc);

static void BM_Native(benchmark::State& aState)
{
    const std::function<bool(int)> sMethod([](int x) {
        return (x > 5 && x < 20) || (x > 20 && x < 30);
    });

    int sParam = 25;
    for (auto _ : aState) {
        bool sResult = sMethod(sParam);
        benchmark::DoNotOptimize(sResult);
        sParam++;
        sParam %= 40;
    }
}
BENCHMARK(BM_Native);

BENCHMARK_MAIN();
