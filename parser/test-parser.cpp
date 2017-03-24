#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <Parser.hpp>

#if 0
g++ test-parser.cpp -std=c++14 -I . -lboost_unit_test_framework
#endif

BOOST_AUTO_TEST_SUITE(Parser)
BOOST_AUTO_TEST_CASE(simple)
{
    std::string line="asd,zxc,123,";
    {
        std::array<boost::string_ref, 4> result = {"z", "z", "z", "z"};
        BOOST_CHECK(Parse::simple(line, result));
        std::array<boost::string_ref, 4> expected = {"asd", "zxc", "123", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }

    {
        std::array<boost::string_ref, 2> result = {"z", "z"};
        BOOST_CHECK(Parse::simple(line, result));
        std::array<boost::string_ref, 2> expected = {"asd", "zxc"};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
}
BOOST_AUTO_TEST_CASE(quoted)
{
    std::list<std::string> result;
    std::string line="asd,\"foo\",\"Super, \"\"luxurious\"\" truck\",";
    BOOST_CHECK(Parse::quoted(line, [&result]() mutable -> std::string* {
        result.push_back("");
        return &result.back();
    }));

    std::list<std::string> expected = {"asd", "foo", "Super, \"luxurious\" truck", ""};
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
}
BOOST_AUTO_TEST_SUITE_END()
