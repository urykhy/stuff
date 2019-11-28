#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <Parser.hpp>
#include <Atoi.hpp>
#include <Hex.hpp>
#include <ULeb128.hpp>
#include <Autoindex.hpp>

BOOST_AUTO_TEST_SUITE(Parser)
BOOST_AUTO_TEST_CASE(simple)
{
    std::string line="asd,zxc,123,";
    {
        std::vector<boost::string_ref> result;
        BOOST_CHECK(Parse::simple(line, result));
        std::vector<boost::string_ref> expected = {"asd", "zxc", "123"};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
}
BOOST_AUTO_TEST_CASE(quoted)
{
    {
        std::list<std::string> result;
        std::string line="asd,\"foo\",\"Super, \"\"luxurious\"\" truck\",";
        BOOST_CHECK(Parse::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));

        std::list<std::string> expected = {"asd", "foo", "Super, \"luxurious\" truck", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }

    {
        std::list<std::string> result;
        std::string line="asd,\"foo\"";
        BOOST_CHECK(Parse::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));
    }

    {
        std::list<std::string> result;
        std::string line="asd,\"foo";
        BOOST_CHECK(false == Parse::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));
    }
}
BOOST_AUTO_TEST_CASE(escape)
{
    {
        std::list<std::string> result;

        std::string line=R"(asd,f\,oo,Super\, "luxurious" truck,)";
        BOOST_CHECK(Parse::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }, ',', '"', '\\'));

        std::list<std::string> expected = {"asd", "f,oo", "Super, \"luxurious\" truck", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
    {
        std::list<std::string> result;

        std::string line=R"("a\\sd",f"oo,"Super, \"luxurious"" truck",)";
        BOOST_CHECK(Parse::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));

        std::list<std::string> expected = {"a\\sd", "f\"oo", "Super, \"luxurious\" truck", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
    {
        std::list<std::string> result;
        std::string line=R"(asd,foo\)";
        BOOST_CHECK(false == Parse::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));
    }
    {
        std::list<std::string> result;
        std::string line=R"(asd,\"foo,"""bar""")";
        BOOST_CHECK(true == Parse::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));
        auto it = result.begin();
        BOOST_CHECK_EQUAL(*it++, "asd");
        BOOST_CHECK_EQUAL(*it++, "\"foo");
        BOOST_CHECK_EQUAL(*it++, "\"bar\"");
    }
}
BOOST_AUTO_TEST_CASE(hex)
{
    BOOST_CHECK_EQUAL(Parser::to_hex("ABC1="), "414243313d");
    BOOST_CHECK_EQUAL(Parser::from_hex("414243313d"), "ABC1=");
    BOOST_CHECK_EQUAL(Parser::to_hex_c_string("ABC1="), "\\x41\\x42\\x43\\x31\\x3d");

}
BOOST_AUTO_TEST_CASE(atoi)
{
    BOOST_CHECK_EQUAL(Parser::Atoi<int>("123"), 123);
    BOOST_CHECK_THROW(Parser::Atoi<int>("123rt"), Parser::NotNumber);
    BOOST_CHECK_THROW(Parser::Atoi<int>("12.456"), Parser::NotNumber);
    BOOST_CHECK_CLOSE(Parser::Atof<float>("12.456"), 12.456, 0.0001);
    BOOST_CHECK_CLOSE(Parser::Atof<float>("-45"), -45, 0.0001);
}
BOOST_AUTO_TEST_CASE(uleb128)
{
    {
        std::stringstream s;
        Parser::ULEB128::encode(45, s);
        BOOST_CHECK_EQUAL(s.str(), "\x2D");
        BOOST_CHECK_EQUAL(Parser::ULEB128::decode(s), 45);
    }

    {
        std::stringstream s;
        Parser::ULEB128::encode(624485, s);
        BOOST_CHECK_EQUAL(s.str(), "\xE5\x8E\x26");
        BOOST_CHECK_EQUAL(Parser::ULEB128::decode(s), 624485);
    }
}
BOOST_AUTO_TEST_CASE(url)
{
    BOOST_CHECK_EQUAL("https%3a%2f%2fwww.urlencoder.org%2f", Parser::to_url("https://www.urlencoder.org/"));
    BOOST_CHECK_EQUAL("https://www.urlencoder.org/", Parser::from_url("https%3A%2F%2Fwww.urlencoder.org%2F"));
}
BOOST_AUTO_TEST_CASE(autoindex)
{
    const std::string sBody=R"(
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
<html>
 <head>
  <title>Index of /</title>
 </head>
 <body>
<h1>Index of /</h1>
  <table>
   <tr><th valign="top"><img src="/icons/blank.gif" alt="[ICO]"></th><th><a href="?C=N;O=D">Name</a></th><th><a href="?C=M;O=A">Last modified</a></th><th><a href="?C=S;O=A">Size</a></th></tr>
   <tr><th colspan="4"><hr></th></tr>
<tr><td valign="top"><img src="/icons/folder.gif" alt="[DIR]"></td><td><a href="calque/">calque/</a></td><td align="right">2015-08-07 17:52  </td><td align="right">  - </td></tr>
<tr><td valign="top"><img src="/icons/folder.gif" alt="[DIR]"></td><td><a href="dygraf/">dygraf/</a></td><td align="right">2016-07-02 10:04  </td><td align="right">  - </td></tr>
    )";
    const Parse::StringList expected = {"calque/", "dygraf/"};
    const auto result = Parse::Autoindex(sBody);
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
}
BOOST_AUTO_TEST_SUITE_END()
