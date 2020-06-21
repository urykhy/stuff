#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <curl/Curl.hpp>
#include <networking/Resolve.hpp>
#include <threads/WaitGroup.hpp>

#include "Router.hpp"

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(httpd)
BOOST_AUTO_TEST_CASE(parser1)
{
    const std::string sData =
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

    httpd::Request::Handler sHandler = [&sCalled](httpd::Request& aRequest) {
        BOOST_CHECK_EQUAL(aRequest.m_Method, "POST");
        BOOST_CHECK_EQUAL(aRequest.m_Url, "/joyent/http-parser");
        const auto sHeaders = aRequest.m_Headers;
        for (const auto& x : sHeaders) {
            BOOST_TEST_MESSAGE("found header " << x.key << "=" << x.value);
            if (x.key == "DNT")
                BOOST_CHECK_EQUAL(x.value, "1");
        }
        BOOST_CHECK_EQUAL(aRequest.m_Body, "hello world");
        sCalled = true;
    };

    httpd::Parser sParser(sHandler);
    sParser.consume(sData.data(), sData.size());
    BOOST_CHECK(sCalled);
}
BOOST_AUTO_TEST_CASE(parser2)
{
    const std::string sData =
        "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 20\r\n"
        "Content-Type: text/plain\r\n"
        "Date: Sat, 30 May 2020 10:42:02 GMT\r\n"
        "Last-Modified: Thu, 12 Jun 2014 11:54:06 GMT\r\n"
        "Server: nginx/1.14.2\r\n"
        "\r\n"
        "<?php\n"
        "phpinfo();\n"
        "?>\n";

    bool sCalled = false;

    httpd::Response::Handler sHandler = [&sCalled](httpd::Response& aResponse) {
        BOOST_CHECK_EQUAL(aResponse.m_Status, 200);
        const auto sHeaders = aResponse.m_Headers;
        for (const auto& x : sHeaders) {
            BOOST_TEST_MESSAGE("found header " << x.key << "=" << x.value);
            if (x.key == "Server")
                BOOST_CHECK_EQUAL(x.value, "nginx/1.14.2");
        }
        BOOST_CHECK_EQUAL(aResponse.m_Body, "<?php\nphpinfo();\n?>\n");
        sCalled = true;
    };

    httpd::Parser sParser(sHandler);
    sParser.consume(sData.data(), sData.size());
    BOOST_CHECK(sCalled);
}
BOOST_AUTO_TEST_CASE(simple)
{
    Util::EPoll    sEPoll;
    httpd::Router  sRouter;
    Threads::Group sGroup; // in d-tor we stop all threads
    sEPoll.start(sGroup);

    auto sHandler1 = [](httpd::Connection::SharedPtr aPeer, const Request& aRequest) {
        BOOST_TEST_MESSAGE("request: " << aRequest.m_Url);
        std::string sResponse =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 10\r\n"
            "Content-Type: text/numbers\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "0123456789";
        aPeer->write(sResponse);
        return Connection::UserResult::DONE;
    };
    sRouter.insert_sync("/hello", sHandler1); // call handler in network thread

    auto sHandler2 = [](httpd::Connection::SharedPtr aPeer, const Request& aRequest) {
        BOOST_TEST_MESSAGE("request: " << aRequest.m_Url);
        std::string sResponse =
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Content-Type: text/numbers\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "A\r\n" // chunk size in hex
            "9876543210\r\n"
            "0\r\n\r\n";
        aPeer->write(sResponse);
        return Connection::UserResult::DONE;
    };
    sRouter.insert("/async", sHandler2); // call handler in worker thread
    sRouter.start(sGroup);

    auto sListener = httpd::Create(&sEPoll, 2081, sRouter);
    sListener->start();

    std::this_thread::sleep_for(10ms);

    {
        Curl::Client::Params sParams;
        Curl::Client         sClient(sParams);

        auto sResult = sClient.GET("http://127.0.0.1:2081/hello");
        BOOST_CHECK_EQUAL(sResult.first, 200);
        BOOST_CHECK_EQUAL(sResult.second, "0123456789");
        sResult = sClient.GET("http://127.0.0.1:2081/async");
        BOOST_CHECK_EQUAL(sResult.first, 200);
        BOOST_CHECK_EQUAL(sResult.second, "9876543210");
        sResult = sClient.GET("http://127.0.0.1:2081/not_exists");
        BOOST_CHECK_EQUAL(sResult.first, 404);
    }

    {
        int sResponseCount = 0;

        auto sClient = std::make_shared<ClientConnection>(&sEPoll, Tcp::Socket(), [&sResponseCount](ClientConnection::SharedPtr aPeer, const Response& aResponse) {
            sResponseCount++;
            BOOST_TEST_MESSAGE("response " << sResponseCount);
            BOOST_TEST_MESSAGE("code: " << aResponse.m_Status);
            for (const auto& x : aResponse.m_Headers)
                BOOST_TEST_MESSAGE("found header " << x.key << "=" << x.value);
            BOOST_TEST_MESSAGE("body: " << aResponse.m_Body);
            return ClientConnection::UserResult::DONE;
        });
        Threads::WaitGroup sWait(1);
        sClient->connect(Util::resolveAddr("127.0.0.1"), 2081, 10 /* timeout in ms */, [&sWait](int) {
            sWait.release();
        });
        sWait.wait();
        BOOST_CHECK_EQUAL(sClient->is_connected(), 0);

        sClient->write("GET /hello HTTP/1.1\r\nConnection: keep-alive\r\n"
                       "\r\n"
                       "GET /async HTTP/1.1\r\nConnection: keep-alive\r\n"
                       "\r\n"
                       "GET /not_exists HTTP/1.1\r\nConnection: keep-alive\r\n"
                       "\r\n");
        std::this_thread::sleep_for(20ms);
        BOOST_CHECK_EQUAL(sResponseCount, 3);
    }
    std::this_thread::sleep_for(10ms);
}
BOOST_AUTO_TEST_SUITE_END()
