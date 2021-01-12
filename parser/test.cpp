#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include "Parser.hpp"
#include "Atoi.hpp"
#include "Hex.hpp"
#include "Url.hpp"
#include "format/Base64.hpp"
#include <format/Hex.hpp>
#include <format/Url.hpp>
#include <format/ULeb128.hpp>
#include "ULeb128.hpp"
#include "Autoindex.hpp"
#include "Base64.hpp"
#include "Multipart.hpp"

BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_CASE(simple)
{
    std::string line="asd,zxc,123,";
    {
        std::vector<std::string_view> result;
        BOOST_CHECK(Parser::simple(line, result));
        std::vector<std::string_view> expected = {"asd", "zxc", "123"};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
}
BOOST_AUTO_TEST_CASE(quoted)
{
    {
        std::list<std::string> result;
        std::string line="asd,\"foo\",\"Super, \"\"luxurious\"\" truck\",";
        BOOST_CHECK(Parser::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));

        std::list<std::string> expected = {"asd", "foo", "Super, \"luxurious\" truck", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }

    {
        std::list<std::string> result;
        std::string line="asd,\"foo\"";
        BOOST_CHECK(Parser::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));
    }

    {
        std::list<std::string> result;
        std::string line="asd,\"foo";
        BOOST_CHECK(false == Parser::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));
    }
}
BOOST_AUTO_TEST_CASE(escape)
{
    {
        std::list<std::string> result;

        std::string line=R"(asd,f\,oo,Super\, "luxurious" truck,)";
        BOOST_CHECK(Parser::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }, ',', '"', '\\'));

        std::list<std::string> expected = {"asd", "f,oo", "Super, \"luxurious\" truck", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
    {
        std::list<std::string> result;

        std::string line=R"("a\\sd",f"oo,"Super, \"luxurious"" truck",)";
        BOOST_CHECK(Parser::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));

        std::list<std::string> expected = {"a\\sd", "f\"oo", "Super, \"luxurious\" truck", ""};
        BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
    }
    {
        std::list<std::string> result;
        std::string line=R"(asd,foo\)";
        BOOST_CHECK(false == Parser::quoted(line, [&result](std::string& aStr) mutable {
            result.push_back(aStr);
        }));
    }
    {
        std::list<std::string> result;
        std::string line=R"(asd,\"foo,"""bar""")";
        BOOST_CHECK(true == Parser::quoted(line, [&result](std::string& aStr) mutable {
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
    BOOST_CHECK_EQUAL(Format::to_hex("ABC1="), "414243313d");
    BOOST_CHECK_EQUAL(Parser::from_hex("414243313d"), "ABC1=");
    BOOST_CHECK_EQUAL(Format::to_hex_c_string("ABC1="), "\\x41\\x42\\x43\\x31\\x3d");

    BOOST_CHECK_EQUAL(Parser::from_hex_mixed("ABC"), "ABC");
    BOOST_CHECK_EQUAL(Parser::from_hex_mixed("\\x41B\\n"), "AB\n");
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
        Format::uleb128(45, s);
        BOOST_CHECK_EQUAL(s.str(), "\x2D");
        BOOST_CHECK_EQUAL(Parser::uleb128(s), 45);
    }

    {
        std::stringstream s;
        Format::uleb128(624485, s);
        BOOST_CHECK_EQUAL(s.str(), "\xE5\x8E\x26");
        BOOST_CHECK_EQUAL(Parser::uleb128(s), 624485);
    }
}
BOOST_AUTO_TEST_CASE(url)
{
    BOOST_CHECK_EQUAL("https%3a%2f%2fwww.urlencoder.org%2f", Format::url_encode("https://www.urlencoder.org/"));
    BOOST_CHECK_EQUAL("https://www.urlencoder.org/", Parser::url_decode("https%3A%2F%2Fwww.urlencoder.org%2F"));

    auto sParsed = Parser::url("http://localhost/ping");
    BOOST_CHECK_EQUAL(sParsed.host, "localhost");
    BOOST_CHECK_EQUAL(sParsed.port, "80");
    BOOST_CHECK_EQUAL(sParsed.query, "/ping");

    sParsed = Parser::url("http://localhost:2345/pong");
    BOOST_CHECK_EQUAL(sParsed.host, "localhost");
    BOOST_CHECK_EQUAL(sParsed.port, "2345");
    BOOST_CHECK_EQUAL(sParsed.query, "/pong");

    sParsed = Parser::url("http://localhost");
    BOOST_CHECK_EQUAL(sParsed.query, "/");
}
BOOST_AUTO_TEST_CASE(multipart)
{
    // boundary is "--" + Content-Type/boundary
    const std::string_view sBoundary = "--" "------------------------30476e420cacf245";
    const std::string_view sData =
R"(--------------------------30476e420cacf245
Content-Disposition: form-data; name="key1"

value1
--------------------------30476e420cacf245
Content-Disposition: form-data; name="upload"; filename="data.bin"
Content-Type: application/octet-stream

some
binary
data

or
simple text

--------------------------30476e420cacf245--)";

    unsigned sCounter = 0;
    Parser::multipart(sData, sBoundary, [&sCounter](auto aMeta, std::string_view aData){
        sCounter++;
        switch (sCounter)
        {
        case 1:
            BOOST_CHECK_EQUAL(aMeta.name, "key1");
            BOOST_CHECK_EQUAL(aData, "value1");
            break;
        case 2:
            BOOST_CHECK_EQUAL(aMeta.name, "upload");
            BOOST_CHECK_EQUAL(aMeta.filename, "data.bin");
            BOOST_CHECK_EQUAL(aMeta.content_type, "application/octet-stream");
            BOOST_CHECK_EQUAL(aData.size(), 33);
            break;
        };
    });
    BOOST_CHECK_EQUAL(sCounter, 2);
}
BOOST_AUTO_TEST_CASE(path_params)
{
    const std::string_view sPath = "/api/v1/kv/123?timestamp=456";
    const std::string_view sPattern = "/api/v1/kv/{id}";

    size_t sCount = 0;
    Parser::http_path_params(sPath, sPattern, [&sCount](auto aName, auto aValue){
        BOOST_CHECK_EQUAL(aName, "id");
        BOOST_CHECK_EQUAL(aValue, "123");
        sCount++;
    });
    BOOST_CHECK_EQUAL(sCount, 1);
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
    const Parser::StringList expected = {"calque/", "dygraf/"};
    const auto result = Parser::Autoindex(sBody);
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
}
BOOST_AUTO_TEST_CASE(base64)
{
    const std::string sStr = Parser::from_hex("31ff");
    const std::string sBase64 = Format::Base64(sStr);
    BOOST_CHECK_EQUAL("Mf8=", sBase64);
    BOOST_CHECK_EQUAL(Format::to_hex(sStr), Format::to_hex(Parser::Base64(sBase64)));
}
BOOST_AUTO_TEST_SUITE_END()
