#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <list>
#include <sstream>

#include "Float.hpp"
#include "Hex.hpp"
#include "Json.hpp"
#include "List.hpp"

BOOST_AUTO_TEST_SUITE(Format)
BOOST_AUTO_TEST_CASE(list)
{
    std::stringstream a;
    std::list<int>    list{1, 2, 3};
    Format::List(a, list);
    BOOST_CHECK_EQUAL(a.str(), "1, 2, 3");

    a.str("");
    Format::List(a, list, [](int x) -> std::string { return std::to_string(x * x); });
    BOOST_CHECK_EQUAL(a.str(), "1, 4, 9");
}
BOOST_AUTO_TEST_CASE(float_precision)
{
    BOOST_CHECK_EQUAL("123.46", Format::with_precision(123.4567890, 2));
    BOOST_CHECK_EQUAL("123.00", Format::with_precision(123, 2));
    BOOST_CHECK_EQUAL("0.12", Format::with_precision(0.123456, 2));
}
BOOST_AUTO_TEST_CASE(json)
{
    using namespace Format::Json;

    struct X
    {
        int   a = 0;
        int   b = 0;
        Value to_json() const
        {
            Value sValue(::Json::objectValue);
            sValue["a"] = to_value(a);
            sValue["b"] = to_value(b);
            return sValue;
        }
    };

    Value sJson;
    write(sJson, "number", 42);
    write(sJson, "real", 42.5);
    write(sJson, "str", std::string_view("12345", 5));

    std::vector<std::string> sList{"one", "two", "three"};
    write(sJson, "str-array", sList);

    std::optional<int> sOpt1;
    std::optional<int> sOpt2{3};
    write(sJson, "opt1", sOpt1);
    write(sJson, "opt2", sOpt2);

    X sX{11, 12};
    write(sJson, "x-struct", sX);

    const std::string sResult = to_string(sJson);
    BOOST_TEST_MESSAGE(sResult);
}
BOOST_AUTO_TEST_SUITE_END()
