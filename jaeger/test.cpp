#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include <thread>

#include "Client.hpp"
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
    using Tag = Jaeger::Tag;
    Jaeger::Queue sJaeger("test.cpp", "0.1/test");
    sJaeger.start();

    Jaeger::Trace sTrace(Jaeger::Params::uuid());

    {
        Jaeger::Span sM(sTrace, "initialize");
        std::this_thread::sleep_for(100us);
    }
    {
        Jaeger::Span sM(sTrace, "download");
        std::this_thread::sleep_for(100us);
    }

    Jaeger::Span sProcess(sTrace, "process");
    {
        {
            Jaeger::Span sM(sProcess.child("fetch"));
            std::this_thread::sleep_for(200us);
        }
        {
            Jaeger::Span sM(sProcess.child("merge"));
            std::this_thread::sleep_for(300us);
            sM.set_log("merge", Tag{"factor", 42.2}, Tag{"duplicates", 50l}, Tag{"unique", 10l}, Tag{"truncated", 4l});
        }
        try {
            Jaeger::Span sM(sProcess.child("write"));
            std::this_thread::sleep_for(300us);
            throw 1;
        } catch (...) {
        }
    }
    sProcess.set_tag(Tag{"result", "success"});
    sProcess.set_tag(Tag{"count", 50l});
    sProcess.close();

    {
        Jaeger::Span sM(sTrace, "commit");
        std::this_thread::sleep_for(10us);
    }

    sJaeger.send(sTrace);
    waitTrace(sTrace.trace_id());
}
BOOST_AUTO_TEST_CASE(parts)
{
    using Tag = Jaeger::Tag;
    Jaeger::Queue sJaeger("test.cpp", "0.1/test");
    sJaeger.start();

    Jaeger::Trace sTrace(Jaeger::Params::uuid());

    Jaeger::Span sRoot(sTrace, "root");
    {
        Jaeger::Span sCall = sRoot.child("call");
        std::this_thread::sleep_for(30ms);

        auto sState = sCall.extract();
        BOOST_TEST_MESSAGE("parent: " << sState.traceparent());

        // remote side
        Jaeger::Queue sRemoteJaeger("s3", "0.2/test");
        sRemoteJaeger.start();
        Jaeger::Trace sTrace2(sState);
        {
            Jaeger::Span sPerform(sTrace2, "perform");
            sPerform.set_tag(Tag{"remote", true});
            std::this_thread::sleep_for(2ms);
            {
                Jaeger::Span sStore(sPerform.child("store"));
                std::this_thread::sleep_for(5ms);
            }
            std::this_thread::sleep_for(2ms);
        }
        sRemoteJaeger.send(sTrace2);
        std::this_thread::sleep_for(10ms);
    }
    sRoot.close();
    sJaeger.send(sTrace);
}
BOOST_AUTO_TEST_SUITE_END()
