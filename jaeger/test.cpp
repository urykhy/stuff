#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include <thread>

#include "Client.hpp"
#include "Helper.hpp"
#include "Jaeger.hpp"

static void waitTrace(const std::string& aTraceID)
{
    bool sTraceFound = false;
    auto sCheckTrace = [&]() {
        std::string  sURL = Util::getEnv("JAEGER_QUERY") + "/" + aTraceID;
        Curl::Client sClient;
        BOOST_TEST_MESSAGE("wait for trace at " << sURL);
        return sClient.GET(sURL).status == 200;
    };
    for (int i = 0; i < 10 and !sTraceFound; i++) {
        Threads::sleep(1);
        sTraceFound = sCheckTrace();
    }
    BOOST_CHECK_EQUAL(sTraceFound, true);
}

BOOST_AUTO_TEST_SUITE(Jaeger)
BOOST_AUTO_TEST_CASE(params)
{
    const auto sParams = Jaeger::Params::uuid();
    BOOST_TEST_MESSAGE("parent: " << sParams.traceparent());

    const auto sParsed = Jaeger::Params::parse(sParams.traceparent());

    BOOST_CHECK_EQUAL(sParsed.traceIdHigh, sParams.traceIdHigh);
    BOOST_CHECK_EQUAL(sParsed.traceIdLow, sParams.traceIdLow);
    BOOST_CHECK_EQUAL(sParsed.parentId, sParams.parentId);
}
BOOST_AUTO_TEST_CASE(simple)
{
    using namespace Jaeger;
    auto sQueue = std::make_shared<Queue>("test.cpp", "0.1/test");
    sQueue->start();

    std::string sTraceId;
    {
        auto sTrace = start(Jaeger::Params::uuid(), sQueue, "root");
        sTraceId    = sTrace->trace_id();
        {
            auto sM = start(sTrace, "initialize");
            std::this_thread::sleep_for(100us);
        }
        {
            auto sM = start(sTrace, "download");
            std::this_thread::sleep_for(100us);
        }

        auto sProcess = start(sTrace, "process");
        {
            {
                auto sM = start(sProcess, "fetch");
                std::this_thread::sleep_for(200us);
            }
            {
                auto sM = start(sProcess, "merge");
                std::this_thread::sleep_for(150us);
                set_log(sM, "merge1");
                std::this_thread::sleep_for(150us);
                set_log(sM, "merge2", Tag{"factor", 42.2}, Tag{"duplicates", 50l}, Tag{"unique", 10l}, Tag{"truncated", 4l});
                std::this_thread::sleep_for(150us);
            }
            try {
                auto sM = start(sProcess, "write");
                std::this_thread::sleep_for(300us);
                throw 1;
            } catch (...) {
            }
        }
        set_tag(sProcess, Tag{"result", "success"});
        set_tag(sProcess, Tag{"count", 50l});

        {
            auto sM = start(sTrace, "commit");
            std::this_thread::sleep_for(10us);
        }
    }
    waitTrace(sTraceId);
}
BOOST_AUTO_TEST_CASE(parts)
{
    using namespace Jaeger;

    auto sQueue = std::make_shared<Queue>("test.cpp", "0.1/test");
    sQueue->start();

    {
        auto sRoot = start(Params::uuid(), sQueue, "root");
        auto sCall = start(sRoot, "call");
        std::this_thread::sleep_for(30ms);

        auto sTraceParent = sCall->traceparent();
        BOOST_TEST_MESSAGE("parent: " << sTraceParent);

        // remote side
        auto sRemoteQueue = std::make_shared<Queue>("s3", "0.2/test");
        sRemoteQueue->start();
        {
            auto sPerform = start(sTraceParent, sRemoteQueue, "perform");
            sPerform->set_tag(Tag{"remote", true});
            std::this_thread::sleep_for(2ms);
            {
                auto sStore = start(sPerform, "store");
                std::this_thread::sleep_for(5ms);
            }
            std::this_thread::sleep_for(2ms);
        }
        std::this_thread::sleep_for(10ms);
    }
    std::this_thread::sleep_for(10ms);
}
BOOST_AUTO_TEST_SUITE_END()
