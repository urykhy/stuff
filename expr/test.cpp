#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Expr.hpp"

BOOST_AUTO_TEST_SUITE(ExprTest)
BOOST_AUTO_TEST_CASE(OpCache)
{
    Expr::OpCache sCache(129);
    BOOST_CHECK_EQUAL(sCache.m_Cache.size(), 17);
    sCache.Set(0);
    BOOST_CHECK_EQUAL(sCache.Get(0), 1);
    BOOST_CHECK_EQUAL(sCache.Get(1), 0);
    BOOST_CHECK_EQUAL(sCache.m_Cache[0], 1);

    sCache.Set(1);
    BOOST_CHECK_EQUAL(sCache.m_Cache[0], 3);

    BOOST_CHECK_EQUAL(sCache.Get(10), 0);
    sCache.Set(10);
    BOOST_CHECK_EQUAL(sCache.Get(10), 1);
    BOOST_CHECK_EQUAL(sCache.m_Cache[1], 1 << 2);
}
BOOST_AUTO_TEST_CASE(WithInterning)
{
    Expr::UserData sData;
    sData.m_Params["source-A"]["param-A1"] = 22;
    sData.Prepare();

    BOOST_CHECK_EQUAL(sData.m_Dict.size(), 1);
    BOOST_CHECK_EQUAL(sData.m_Params["source-A"]["param-A1"].as<int>(),
                      Expr::Compiler::tccGetInt(static_cast<void*>(&sData), sData.m_Dict("source-A.param-A1"), "source-A.param-A1"));

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

    BOOST_CHECK_EQUAL(sCompiler.Call(sData), 1);
}
BOOST_AUTO_TEST_CASE(WithOpCache)
{
    Expr::UserData sData;
    sData.m_Params["source-A"]["param-A1"] = 22;
    sData.Prepare();

    // call with tcc
    const std::string sCode = R"(
    extern int getIntValue(void*, int, const char*);
    extern int isCached(void*, int);
    extern int readCache(void*, int);
    extern int storeCache(void*, int, int);
    int calcSegment(void* data) {
        const int x = getIntValue(data, 0, "source-A.param-A1");
        return (isCached(data, 0) ? readCache(data, 0) : storeCache(data, 0, (x > 5 && x < 20)))
            || (isCached(data, 1) ? readCache(data, 1) : storeCache(data, 1, (x > 20 && x < 30)));
    }
    )";
    Expr::Compiler    sCompiler;
    sCompiler.Compile(sCode, "calcSegment");

    BOOST_CHECK_EQUAL(sCompiler.Call(sData), 1);
}
BOOST_AUTO_TEST_SUITE_END()
