#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include <sol/debug.hpp>
#include <sol/sol.hpp>
#pragma GCC diagnostic pop

#include <unsorted/Tcc.hpp>

BOOST_AUTO_TEST_SUITE(Lua)
BOOST_AUTO_TEST_CASE(Simple)
{
    sol::state sLua;
    sLua.open_libraries(sol::lib::base);
    sol::detail::debug::print_lua_information(sLua);

    sLua.script(R"(
        function test(x)
          return x > 5 and x < 20
        end
        v1 = test(10)
        print (v1)
    )");
    BOOST_CHECK_EQUAL(true, sLua.get<bool>("v1"));

    sol::function sMethod = sLua["test"];
    bool          sResult = sMethod(11);
    BOOST_CHECK_EQUAL(true, sResult);
    sResult = sMethod(21);
    BOOST_CHECK_EQUAL(false, sResult);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Tcc)
BOOST_AUTO_TEST_CASE(Simple)
{
    const std::string  sCode = R"(
    int test1(int x) {
        return x > 5 && x < 20;
    }
    )";
    Util::TinyCompiler sCompiler;
    sCompiler.Compile(sCode);

    using T = int (*)(int);
    T sPtr  = (T)sCompiler.GetSymbol("test1");
    if (!sPtr) {
        throw std::invalid_argument("fail to get symbol");
    }
    BOOST_CHECK_EQUAL(sPtr(10), 1);
}
BOOST_AUTO_TEST_SUITE_END()
