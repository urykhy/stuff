#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>

#include "Curl.hpp"
#include "Multi.hpp"
#include "Util.hpp"

#include <threads/Periodic.hpp> // for sleep
#include <time/Meter.hpp>
#include <unsorted/Process.hpp>
using namespace std::chrono_literals;

namespace bp = boost::process;
class WithServer
{
    bp::ipstream m_Stream;
    bp::child    m_Server;

public:
    WithServer()
    : m_Server(Util::Spawn(bp::search_path("httpd.py", {".", ".."}), bp::std_in.close(), bp::std_out > bp::null, bp::std_err > m_Stream))
    {
        BOOST_TEST_MESSAGE("server running as " << m_Server.id());
        bool        sReady = false;
        std::string sLine;
        while (!sReady and m_Server.running() and std::getline(m_Stream, sLine) and !sLine.empty()) {
            BOOST_TEST_MESSAGE(sLine);
            if (std::string::npos != sLine.find("Bus STARTED"))
                sReady = true;
        }
    }
};

BOOST_FIXTURE_TEST_SUITE(Curl, WithServer)
BOOST_AUTO_TEST_CASE(Basic)
{
    using R = Curl::Client::Request;

    Curl::Client sClient;
    auto         sResult = sClient.GET("http://127.0.0.1:8088/hello");
    BOOST_CHECK_EQUAL(sResult.status, 200);
    BOOST_CHECK_EQUAL(sResult.body, "Hello World!");
    BOOST_CHECK_EQUAL(String::starts_with(sResult.headers["Server"], "CherryPy/"), true);

    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8088/useragent").body, "Curl++");

    BOOST_CHECK_EQUAL(
        sClient(R{.url     = "http://127.0.0.1:8088/header",
                  .headers = {{"XFF", "value"}}})
            .body,
        "value");

    BOOST_CHECK_EQUAL(
        sClient(R{.url    = "http://127.0.0.1:8088/cookie",
                  .cookie = "name1=content1;"})
            .body,
        "content1");

    BOOST_CHECK_EQUAL(
        sClient(R{.url      = "http://127.0.0.1:8088/auth",
                  .username = "name",
                  .password = "secret"})
            .status,
        200);

    BOOST_CHECK_EQUAL(
        sClient(R{.url      = "http://127.0.0.1:8088/auth",
                  .username = "othername",
                  .password = "secret"})
            .status,
        401);

    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8088/nx_location").status, 404);
    BOOST_CHECK_EQUAL(sClient.HEAD("http://127.0.0.1:8088/nx_location"), 404);
    BOOST_CHECK_THROW(sClient.GET("http://127.0.0.1:8088/slow"), Curl::Client::Error);
}
BOOST_AUTO_TEST_CASE(Methods)
{
    Curl::Client sClient;

    auto sResult = sClient.POST("http://127.0.0.1:8088/post_handler", "W=123");
    BOOST_CHECK_EQUAL(sResult.status, 200);
    BOOST_CHECK_EQUAL(sResult.body, "post: 123");

    sResult = sClient.PUT("http://127.0.0.1:8088/method_handler", "some data");
    BOOST_CHECK_EQUAL(sResult.status, 200);
    BOOST_CHECK_EQUAL(sResult.body, "PUT: some data");

    sResult = sClient.DELETE("http://127.0.0.1:8088/method_handler");
    BOOST_CHECK_EQUAL(sResult.status, 200);
    BOOST_CHECK_EQUAL(sResult.body, "OK");
}
BOOST_AUTO_TEST_CASE(Stream)
{
    Curl::Client sClient;
    std::string  sResult;
    int          sStatus = sClient.GET("http://127.0.0.1:8088/hello", [&sResult](void* aPtr, size_t aSize) -> size_t {
        sResult.append((char*)aPtr, aSize);
        return aSize;
    });
    BOOST_CHECK_EQUAL(sStatus, 200);
    BOOST_CHECK_EQUAL(sResult, "Hello World!");
}
BOOST_AUTO_TEST_CASE(Async)
{
    std::atomic_bool    ok{false};
    Curl::Multi::Params sParams;
    Curl::Multi         sClient(sParams);
    Threads::Group      tg;
    sClient.start(tg);

    sClient.GET("http://127.0.0.1:8088/hello", [&ok](Curl::Multi::Result&& aResult) {
        try {
            const auto sResult = aResult.get();
            BOOST_CHECK_EQUAL(sResult.status, 200);
            BOOST_CHECK_EQUAL(sResult.body, "Hello World!");
            ok = true;
        } catch (const std::exception& e) {
            BOOST_TEST_MESSAGE("got exception: " << e.what());
        }
    });

    for (int i = 0; i < 10 and !ok; i++) {
        Threads::sleep(0.1);
    }
    BOOST_CHECK(ok);
}
BOOST_AUTO_TEST_CASE(InQueueTimeout)
{
    std::atomic_bool ok1{false};
    std::atomic_bool ok2{false};

    Curl::Multi::Params sParams;
    sParams.max_connections  = 1;
    sParams.queue_timeout_ms = 1000;
    Curl::Multi    sClient(sParams);
    Threads::Group tg;
    sClient.start(tg);

    sClient.GET("http://127.0.0.1:8088/slow", [&ok1](Curl::Multi::Result&& aResult) {
        try {
            aResult.get();
        } catch (const Curl::Client::Error& e) {
            BOOST_REQUIRE_EQUAL(e.what(), "Timeout was reached");
            ok1 = 1;
        }
    });

    sClient.GET("http://127.0.0.1:8088/hello", [&ok2](Curl::Multi::Result&& aResult) {
        try {
            aResult.get();
        } catch (const Curl::Client::Error& e) {
            BOOST_REQUIRE_EQUAL(e.what(), "timeout in queue");
            ok2 = 1;
        }
    });

    for (int i = 0; i < 50 and (!ok1 or !ok2); i++) {
        Threads::sleep(0.1);
    }
    BOOST_CHECK_MESSAGE(ok1, "first call with request timeout");
    BOOST_CHECK_MESSAGE(ok2, "second call in-queue timeout");
}
BOOST_AUTO_TEST_CASE(Tmp)
{
    auto sTmp = Curl::download("http://127.0.0.1:8088/hello");
    BOOST_TEST_MESSAGE("downloaded to " << sTmp.name());
    BOOST_CHECK_EQUAL(sTmp.size(), 12);

    // step 2. massive loader
    std::mutex         sMutex;
    uint64_t           sCounter = 0;
    Parser::StringList sUrls{"http://127.0.0.1:8088/hello", "http://127.0.0.1:8088/hello", "http://127.0.0.1:8088/hello"};
    Curl::download(sUrls, 2, [&sMutex, &sCounter](const std::string& aUrl, File::Tmp& sTmp) mutable {
        std::unique_lock<std::mutex> lk(sMutex);
        sCounter++;
        BOOST_TEST_MESSAGE("mass download " << aUrl << " to " << sTmp.name());
        BOOST_CHECK_EQUAL(sTmp.size(), 12);
    });
    BOOST_CHECK_EQUAL(sCounter, sUrls.size());

    // step3. get download error
    Parser::StringList sUrls2{"http://127.0.0.1:8088/hello", "http://127.0.0.1:8088/nx_location"};
    BOOST_CHECK_THROW(Curl::download(sUrls2, 2, [](const std::string& aUrl, File::Tmp& sTmp) mutable {}), Curl::Client::Error);
}
BOOST_AUTO_TEST_CASE(Index)
{
    {
        auto [sValid, sFiles]        = Curl::index("http://127.0.0.1:8088/auto_index");
        Parser::StringList sExpected = {{"../"}, {"20200331"}, {"20200401"}};
        BOOST_CHECK_EQUAL(sValid, true);
        BOOST_CHECK_EQUAL_COLLECTIONS(sExpected.begin(), sExpected.end(), sFiles.begin(), sFiles.end());
    }
    { // check IMS
        auto [sValid, sFiles] = Curl::index("http://127.0.0.1:8088/auto_index", ::time(nullptr));
        BOOST_CHECK_EQUAL(sValid, false);
        BOOST_CHECK_EQUAL(sFiles.empty(), true);
    }
}
BOOST_AUTO_TEST_CASE(Mass)
{
    Curl::Client::GlobalInit();

    const unsigned  REQUEST_COUNT = 5000;
    std::atomic_int sDone{0};

    Curl::Multi::Params sParams; // default 3 sec per request, 32 connections
    Curl::Multi         sClient(sParams);
    Threads::Group      tg;
    sClient.start(tg);

    // run against nginx/apache. cherry py is slow
    Time::Meter sMeter;
    auto        spawn = [&sDone, &sClient]() {
        sClient.GET("http://127.0.0.1:80/local.html", [&sDone](Curl::Multi::Result&& aResult) {
            try {
                const auto sResult = aResult.get();
                BOOST_CHECK_EQUAL(sResult.status, 200);
                BOOST_CHECK_EQUAL(sResult.body.size(), 1420);
            } catch (const std::exception& e) {
                BOOST_TEST_MESSAGE("fail to make query: " << e.what());
            }
            sDone++;
        });
    };

    for (unsigned i = 0; i < REQUEST_COUNT; i++)
        spawn();

    // wait
    for (unsigned i = 0; i < 1000 and REQUEST_COUNT != sDone; i++) {
        Threads::sleep(0.01);
    }
    auto sUsed = sMeter.get().to_double();
    std::cerr << "make " << REQUEST_COUNT << " requests in " << sUsed << " seconds, rps: " << REQUEST_COUNT / sUsed << std::endl;
    BOOST_CHECK(sDone == REQUEST_COUNT);
}
BOOST_AUTO_TEST_SUITE_END()
