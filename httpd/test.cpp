#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <curl/Curl.hpp>
#include <networking/Resolve.hpp>
#include <threads/WaitGroup.hpp>

#include "Router.hpp"

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_CASE(request)
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
BOOST_AUTO_TEST_CASE(response)
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
BOOST_AUTO_TEST_SUITE_END()

struct WithServer
{
    Util::EPoll     m_EPoll;
    httpd::Router   m_Router;
    Threads::Group  m_Group; // in d-tor we stop all threads
    static uint64_t m_RequestCount;

    WithServer()
    {
        using namespace httpd;
        m_EPoll.start(m_Group);

        auto sHandler1 = [](Connection::SharedPtr aPeer, const Request& aRequest) {
            //BOOST_TEST_MESSAGE("request: " << aRequest.m_Url);
            m_RequestCount++;
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
        m_Router.insert_sync("/hello", sHandler1); // call handler in network thread

        auto sHandler2 = [](Connection::SharedPtr aPeer, const Request& aRequest) {
            //BOOST_TEST_MESSAGE("request: " << aRequest.m_Url);
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
        m_Router.insert("/async", sHandler2); // call handler in worker thread
        m_Router.start(m_Group);

        auto sListener = httpd::Create(&m_EPoll, 2081, m_Router);
        sListener->start();
        std::this_thread::sleep_for(10ms);
    }
};
uint64_t WithServer::m_RequestCount = 0;

BOOST_FIXTURE_TEST_SUITE(httpd, WithServer)
BOOST_AUTO_TEST_CASE(curl)
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

BOOST_AUTO_TEST_CASE(raw)
{
    Util::EPoll    sEPoll;
    Threads::Group sGroup;
    sEPoll.start(sGroup);

    int sResponseCount = 0;

    auto               sClient = std::make_shared<ClientConnection>(&sEPoll, [&sResponseCount](ClientConnection::SharedPtr aPeer, const Response& aResponse) {
        sResponseCount++;
        BOOST_TEST_MESSAGE("response " << sResponseCount);
        BOOST_TEST_MESSAGE("status:  " << aResponse.m_Status);
        for (const auto& x : aResponse.m_Headers)
            BOOST_TEST_MESSAGE("found header " << x.key << "=" << x.value);
        BOOST_TEST_MESSAGE("body:    " << aResponse.m_Body);
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
BOOST_AUTO_TEST_CASE(simple)
{
    Util::EPoll    sEPoll;
    Threads::Group sGroup;
    sEPoll.start(sGroup);

    httpd::MassClient::Params sParams;
    sParams.remote_addr = Util::resolveAddr("127.0.0.1");
    sParams.remote_port = 2081;
    auto sClient        = std::make_shared<httpd::MassClient>(&sEPoll, sParams);

    int sResponseCount = 0;
    sClient->insert({"GET /hello HTTP/1.1\r\nConnection: keep-alive\r\n"
                     "\r\n",
                     [&sResponseCount](int aCode, const Response& aResponse) {
                         sResponseCount++;
                         BOOST_CHECK_EQUAL(aCode, 0);
                         BOOST_CHECK_EQUAL(aResponse.m_Status, 200);
                         BOOST_CHECK_EQUAL(aResponse.m_Body, "0123456789");
                     }});
    std::this_thread::sleep_for(20ms);

    BOOST_CHECK_EQUAL(sClient->is_connected(), 0);
    BOOST_CHECK_EQUAL(sResponseCount, 1);
}
BOOST_AUTO_TEST_CASE(bench)
{
    Util::EPoll    sEPoll;
    Threads::Group sGroup;
    sEPoll.start(sGroup);

    httpd::MassClient::Params sParams;
    sParams.remote_addr        = Util::resolveAddr("127.0.0.1");
    sParams.remote_port        = 2081;
    auto sClient               = std::make_shared<httpd::MassClient>(&sEPoll, sParams);
    WithServer::m_RequestCount = 0;
    // only keep alive mode must be used

    auto               sStart = Time::get_time();
    const unsigned     COUNT  = 1000000;
    Threads::WaitGroup sWait(COUNT);
    unsigned           sSuccess{0};
    for (unsigned i = 0; i < COUNT; i++) {
        sClient->insert({"GET /hello HTTP/1.1\r\nConnection: keep-alive\r\n"
                         "\r\n",
                         [&](int aCode, const Response& aResponse) {
                             if (aCode == 0)
                                 sSuccess++;
                             sWait.release();
                         }});
    }
    sWait.wait();
    auto sDuration = (Time::get_time() - sStart).to_double();
    BOOST_CHECK_EQUAL(WithServer::m_RequestCount, COUNT);
    BOOST_CHECK_EQUAL(sSuccess, COUNT);
    BOOST_TEST_MESSAGE("" << sSuccess << " successful requests from " << COUNT << " done in " << sDuration << " = " << unsigned(sSuccess / sDuration) << " rps");
}
BOOST_AUTO_TEST_SUITE_END()
