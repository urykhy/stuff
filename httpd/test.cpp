#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Parser.hpp"

BOOST_AUTO_TEST_SUITE(Httpd)
BOOST_AUTO_TEST_CASE(parser)
{
    std::string sData = \
    "POST /joyent/http-parser HTTP/1.1\r\n"
    "Host: github.com\r\n"
    "DNT: 1\r\n"
    "Accept-Encoding: gzip, deflate, sdch\r\n"
    "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.6,en;q=0.4\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_1) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/39.0.2171.65 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,"
        "image/webp,*/*;q=0.8\r\n"
    "Referer: https://github.com/joyent/http-parser\r\n"
    "Connection: keep-alive\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Cache-Control: max-age=0\r\n"
    "\r\n"
    "b\r\nhello world\r\n"
    "0\r\n\r\n";

    bool sCalled = false;
    httpd::Request::Handler sHandler=[&sCalled](httpd::Request& aRequest)
    {
        BOOST_CHECK_EQUAL(aRequest.m_Method, "POST");
        BOOST_CHECK_EQUAL(aRequest.m_Url, "/joyent/http-parser");
        const auto sHeaders = aRequest.m_Headers;
        for (const auto& x : sHeaders)
        {
            BOOST_TEST_MESSAGE("found header " << x.key << "=" << x.value);
            if (x.key == "DNT") BOOST_CHECK_EQUAL(x.value, "1");
        }
        BOOST_CHECK_EQUAL(aRequest.m_Body, "hello world");
        sCalled = true;
    };

    httpd::Parser sParser(sHandler);
    sParser.consume(sData.data(), sData.size());
    BOOST_CHECK(sCalled);
}
BOOST_AUTO_TEST_SUITE_END()