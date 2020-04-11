#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Curl.hpp"
#include "Multi.hpp"
#include "Util.hpp"

#include <threads/Periodic.hpp> // for sleep
#include <time/Meter.hpp>
#include <unsorted/Process.hpp>
#include <chrono>
using namespace std::chrono_literals;

namespace bp = boost::process;
class WithServer
{
    bp::ipstream m_Stream;
    bp::child m_Server;
public:
    WithServer()
    : m_Server(Util::Spawn(bp::search_path("httpd.py", {".",".."}), bp::std_in.close(), bp::std_out > bp::null, bp::std_err > m_Stream))
    {
        BOOST_TEST_MESSAGE("server running as " << m_Server.id());
        bool sReady = false;
        std::string sLine;
        while (!sReady and m_Server.running() and std::getline(m_Stream, sLine) and !sLine.empty())
        {
            BOOST_TEST_MESSAGE(sLine);
            if (std::string::npos != sLine.find("Bus STARTED"))
                sReady = true;
        }
    }
};

BOOST_FIXTURE_TEST_SUITE(Curl, WithServer)
BOOST_AUTO_TEST_CASE(Basic)
{
    Curl::Client::Params sParams;
    sParams.headers.push_back({"XFF","value"});
    sParams.cookie   = "name1=content1;";
    sParams.username = "name";
    sParams.password = "secret";

    Curl::Client sClient(sParams);
    auto sResult = sClient.GET("http://127.0.0.1:8080/hello");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "Hello World!");

    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/useragent").second, "Curl/Client");
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/header").second, "value");
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/cookie").second, "content1");
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/auth").first, 200);

    sParams.username = "othername";
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/auth").first, 401);
    BOOST_CHECK_EQUAL(sClient.GET("http://127.0.0.1:8080/nx_location").first, 404);

    BOOST_CHECK_THROW(sClient.GET("http://127.0.0.1:8080/slow"), Curl::Client::Error);
}
BOOST_AUTO_TEST_CASE(Methods)
{
    Curl::Client::Params sParams;
    Curl::Client sClient(sParams);

    auto sResult = sClient.POST("http://127.0.0.1:8080/post_handler", "W=123");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "post: 123");

    sResult = sClient.PUT("http://127.0.0.1:8080/method_handler", "some data");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "PUT: some data");

    sResult = sClient.DELETE("http://127.0.0.1:8080/method_handler");
    BOOST_CHECK_EQUAL(sResult.first, 200);
    BOOST_CHECK_EQUAL(sResult.second, "OK");

}
BOOST_AUTO_TEST_CASE(Stream)
{
    Curl::Client::Params sParams;
    Curl::Client sClient(sParams);
    std::string sResult;
    int sCode = sClient.GET("http://127.0.0.1:8080/hello", [&sResult](void* aPtr, size_t aSize) -> size_t {
        sResult.append((char*)aPtr, aSize);
        return aSize;
    });
    BOOST_CHECK_EQUAL(sCode, 200);
    BOOST_CHECK_EQUAL(sResult, "Hello World!");
}
BOOST_AUTO_TEST_CASE(Async)
{
    std::atomic_bool ok{false};
    Curl::Multi::Params sParams;
    Curl::Multi sClient(sParams);
    Threads::Group tg;
    sClient.start(tg);

    sClient.GET("http://127.0.0.1:8080/hello", [&ok](Curl::Multi::Result&& aResult){
        try {
            const auto sResult = aResult.get();
            BOOST_CHECK_EQUAL(sResult.first, 200);
            BOOST_CHECK_EQUAL(sResult.second, "Hello World!");
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
    sParams.max_connections = 1;
    sParams.queue_timeout_ms = 1000;
    Curl::Multi sClient(sParams);
    Threads::Group tg;
    sClient.start(tg);

    sClient.GET("http://127.0.0.1:8080/slow", [&ok1](Curl::Multi::Result&& aResult){
        try {
            aResult.get();
        } catch (const Curl::Client::Error& e) {
            BOOST_REQUIRE_EQUAL(e.what(), "Timeout was reached");
            ok1 = 1;
        }
    });

    sClient.GET("http://127.0.0.1:8080/hello", [&ok2](Curl::Multi::Result&& aResult){
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
    Curl::Client::Params sParams;
    auto sTmp = Curl::download("http://127.0.0.1:8080/hello", sParams);
    BOOST_TEST_MESSAGE("downloaded to " << sTmp.filename());
    BOOST_CHECK_EQUAL(sTmp.size(), 12);
}
BOOST_AUTO_TEST_CASE(Index)
{
    Curl::Client::Params sParams;
    auto sFiles = Curl::index("http://127.0.0.1:8080/auto_index", sParams);
    Curl::FileList sExpected = {{"../"},{"20200331"},{"20200401"}};
    BOOST_CHECK_EQUAL_COLLECTIONS(sExpected.begin(), sExpected.end(), sFiles.begin(), sFiles.end());

    // check IMS
    sFiles = Curl::index("http://127.0.0.1:8080/auto_index", sParams, ::time(nullptr));
    BOOST_CHECK(sFiles.empty());
}
BOOST_AUTO_TEST_CASE(Mass)
{
    Curl::Client::GlobalInit();

    enum {REQUEST_COUNT = 5000};
    std::atomic_int sDone{0};

    Curl::Multi::Params sParams;    // default 3 sec per request, 32 connections
    Curl::Multi sClient(sParams);
    Threads::Group tg;
    sClient.start(tg);

    // run against nginx/apache. cherry py is slow
    Time::Meter sMeter;
    auto spawn = [&sDone, &sClient](){
        sClient.GET("http://127.0.0.1:2080/local.html", [&sDone](Curl::Multi::Result&& aResult){
            try {
                const auto sResult = aResult.get();
                BOOST_CHECK_EQUAL(sResult.first, 200);
                BOOST_CHECK_EQUAL(sResult.second.size(), 1420);
            } catch (const std::exception& e) {
                BOOST_TEST_MESSAGE("fail to make query: " << e.what());
            }
            sDone++;
        });
    };

    for (int i = 0; i < REQUEST_COUNT; i++)
        spawn();

    // wait
    for (int i = 0; i < 1000 and REQUEST_COUNT != sDone; i++) {
        Threads::sleep(0.01);
    }
    auto sUsed = sMeter.get().to_double();
    std::cerr << "make " << REQUEST_COUNT << " requests in " << sUsed << " seconds, rps: " << REQUEST_COUNT/sUsed << std::endl;
    BOOST_CHECK(sDone == REQUEST_COUNT);
}
BOOST_AUTO_TEST_SUITE_END()
