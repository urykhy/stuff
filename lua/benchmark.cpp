#include <benchmark/benchmark.h>

#include <exprtk.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include <sol/debug.hpp>
#include <sol/sol.hpp>
#pragma GCC diagnostic pop

struct ExprTK
{
    using symbol_table_t = exprtk::symbol_table<double>;
    using expression_t   = exprtk::expression<double>;
    using parser_t       = exprtk::parser<double>;
    using settings_t     = parser_t::settings_t;

    settings_t     m_Settings;
    symbol_table_t m_Table;
    expression_t   m_Expression;
    parser_t       m_Parser;

    ExprTK()
    : m_Settings(settings_t::compile_all_opts)
    , m_Parser(m_Settings)
    {
        m_Expression.register_symbol_table(m_Table);
    }

    void compile(const std::string& aStr)
    {
        if (!m_Parser.compile(aStr, m_Expression))
            throw std::invalid_argument("Exprtk: fail to compile: " + m_Parser.error());
    }
    double eval()
    {
        return m_Expression.value();
    }
};

static void
BM_Exprtk(benchmark::State& aState)
{
    ExprTK sExpr;
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

BENCHMARK_MAIN();
