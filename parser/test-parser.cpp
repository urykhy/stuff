#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <Parser.hpp>
#include <Atoi.hpp>

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

    {
        std::list<std::string> result;
        std::string line="asd,\"foo\"";
        BOOST_CHECK(Parse::quoted(line, [&result]() mutable -> std::string* {
            result.push_back("");
            return &result.back();
        }));
    }

    {
        std::list<std::string> result;
        std::string line="asd,\"foo";
        BOOST_CHECK(false == Parse::quoted(line, [&result]() mutable -> std::string* {
            result.push_back("");
            return &result.back();
        }));
    }
}
BOOST_AUTO_TEST_CASE(escape)
{
    {
        std::list<std::string> result;

        std::string line=R"(asd,f\,oo,Super\, "luxurious" truck,)";
        BOOST_CHECK(Parse::quoted(line, [&result]() mutable -> std::string* {
            result.push_back("");
            return &result.back();
        }, ',', '"', '\\'));

        std::list<std::string> expected = {"asd", "f,oo", "Super, \"luxurious\" truck", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
    {
        std::list<std::string> result;

        std::string line=R"("a\\sd",f"oo,"Super, \"luxurious"" truck",)";
        BOOST_CHECK(Parse::quoted(line, [&result]() mutable -> std::string* {
            result.push_back("");
            return &result.back();
        }));

        std::list<std::string> expected = {"a\\sd", "f\"oo", "Super, \"luxurious\" truck", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
    {
        std::list<std::string> result;
        std::string line=R"(asd,foo\)";
        BOOST_CHECK(false == Parse::quoted(line, [&result]() mutable -> std::string* {
            result.push_back("");
            return &result.back();
        }));
    }
    {
        std::list<std::string> result;
        std::string line=R"(asd,\"foo,"""bar""")";
        BOOST_CHECK(true == Parse::quoted(line, [&result]() mutable -> std::string* {
            result.push_back("");
            return &result.back();
        }));
        auto it = result.begin();
        BOOST_CHECK_EQUAL(*it++, "asd");
        BOOST_CHECK_EQUAL(*it++, "\"foo");
        BOOST_CHECK_EQUAL(*it++, "\"bar\"");
    }
}
BOOST_AUTO_TEST_CASE(atoi)
{
    BOOST_CHECK_EQUAL(Parser::Atoi<int>("123"), 123);
    BOOST_CHECK_THROW(Parser::Atoi<int>("123rt"), Parser::NotNumber);
    BOOST_CHECK_THROW(Parser::Atoi<int>("12.456"), Parser::NotNumber);
    BOOST_CHECK_CLOSE(Parser::Atof<float>("12.456"), 12.456, 0.0001);
    BOOST_CHECK_CLOSE(Parser::Atof<float>("-45"), -45, 0.0001);
}
BOOST_AUTO_TEST_SUITE_END()
