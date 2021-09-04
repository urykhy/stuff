#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include <thread>

#include "Jaeger.hpp"

BOOST_AUTO_TEST_SUITE(Jaeger)
BOOST_AUTO_TEST_CASE(params)
{
    const auto sParams = Jaeger::Params::uuid("test");
    BOOST_TEST_MESSAGE("parent: " << sParams.traceparent());

    const std::string sState = "span_offset=1a";

    const auto sParsed = Jaeger::Params::parse(sParams.traceparent(), sState);

    BOOST_CHECK_EQUAL(sParsed.traceIdHigh, sParams.traceIdHigh);
    BOOST_CHECK_EQUAL(sParsed.traceIdLow, sParams.traceIdLow);
    BOOST_CHECK_EQUAL(sParsed.parentId, sParams.parentId);
    BOOST_CHECK_EQUAL(sParsed.baseId, 0x1a00000000000001);
    BOOST_CHECK_EQUAL(sParsed.service, "");
}
BOOST_AUTO_TEST_CASE(simple)
{
    using Tag = Jaeger::Metric::Tag;

    Jaeger::Metric sMetric(Jaeger::Params::uuid("test.cpp"));
    sMetric.set_process_tag(Tag{"version", "0.1/test"});

    {
        Jaeger::Metric::Step sM(sMetric, "initialize");
        std::this_thread::sleep_for(100us);
    }
    {
        Jaeger::Metric::Step sM(sMetric, "download");
        std::this_thread::sleep_for(100us);
    }

    Jaeger::Metric::Step sProcess(sMetric, "process");
    {
        {
            Jaeger::Metric::Step sM(sProcess.child("fetch"));
            std::this_thread::sleep_for(200us);
        }
        {
            Jaeger::Metric::Step sM(sProcess.child("merge"));
            std::this_thread::sleep_for(300us);
            sM.set_log(Tag{"factor", 42.2}, Tag{"duplicates", 50l}, Tag{"unique", 10l}, Tag{"truncated", 4l});
        }
        try {
            Jaeger::Metric::Step sM(sProcess.child("write"));
            std::this_thread::sleep_for(300us);
            throw 1;
        } catch (...) {
        }
    }
    sProcess.set_tag(Tag{"result", "success"});
    sProcess.set_tag(Tag{"count", 50l});
    sProcess.close();

    {
        Jaeger::Metric::Step sM(sMetric, "commit");
        std::this_thread::sleep_for(10us);
    }

    Jaeger::send(sMetric);
}
BOOST_AUTO_TEST_CASE(parts)
{
    using Tag = Jaeger::Metric::Tag;

    Jaeger::Metric sMetric(Jaeger::Params::uuid("test.cpp"));
    sMetric.set_process_tag(Tag{"version", "0.1/test"});

    Jaeger::Metric::Step sRoot(sMetric, "root");
    {
        Jaeger::Metric::Step sCall = sRoot.child("call");
        std::this_thread::sleep_for(30ms);

        // 50 is starting id for new spans
        auto sState = sCall.extract();
        BOOST_TEST_MESSAGE("parent: " << sState.traceparent());

        // remote side
        sState.baseId = 50;
        sState.service = "s3";
        Jaeger::Metric sMetric2(sState);
        {
            Jaeger::Metric::Step sPerform(sMetric2, "perform");
            sPerform.set_tag(Tag{"remote", true});
            std::this_thread::sleep_for(2ms);
            {
                Jaeger::Metric::Step sStore(sPerform.child("store"));
                std::this_thread::sleep_for(5ms);
            }
            std::this_thread::sleep_for(2ms);
        }
        Jaeger::send(sMetric2);
        std::this_thread::sleep_for(10ms);
    }
    sRoot.close();
    Jaeger::send(sMetric);
}
BOOST_AUTO_TEST_SUITE_END()
