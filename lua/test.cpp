#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include <sol/debug.hpp>
#include <sol/sol.hpp>
#pragma GCC diagnostic pop

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
