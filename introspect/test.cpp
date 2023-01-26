#define BOOST_TEST_MODULE Suites
#include "Test.hpp"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(Introspect)
BOOST_AUTO_TEST_CASE(basic)
{
    const Tmp::Msg1 sMsg{{}, 1, 2.5, "test"}; // workaround inheritance in Msg1
    Mpl::for_each_element(
        [](auto&& x) {
            BOOST_TEST_MESSAGE(x.first << "=" << x.second);
        },
        sMsg.__introspect());

    ::Json::Value sValue = Format::Json::to_value(sMsg);
    std::string   sStr   = Format::Json::to_string(sValue);
    BOOST_TEST_MESSAGE("json: " << sStr);

    Tmp::Msg1 sParsed;
    sValue = Parser::Json::parse(sStr);
    Parser::Json::from_value(sValue, sParsed);
    BOOST_CHECK_EQUAL(sMsg, sParsed);
}
BOOST_AUTO_TEST_SUITE_END()
