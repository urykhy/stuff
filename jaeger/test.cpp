#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include "Jaeger.hpp"
#include <networking/UdpPipe.hpp>

BOOST_AUTO_TEST_SUITE(Jaeger)
BOOST_AUTO_TEST_CASE(simple)
{
    Jaeger::Metric sMetric("test.cpp");
    sMetric.set_process_tag(Jaeger::Metric::Tag{"version","0.1/test"});
    size_t id = 0;
    id = sMetric.start("initialize"); std::this_thread::sleep_for(100us); sMetric.stop(id);
    id = sMetric.start("download");   std::this_thread::sleep_for(200us); sMetric.stop(id);
    id = sMetric.start("process");
    {
        size_t s = 0;
        s = sMetric.start("fetch", id); std::this_thread::sleep_for(200us); sMetric.stop(s);
        s = sMetric.start("merge", id); std::this_thread::sleep_for(300us); sMetric.stop(s);
        sMetric.span_log(s, Jaeger::Metric::Tag{"factor", 42.2});
        s = sMetric.start("write", id); std::this_thread::sleep_for(500us); sMetric.stop(s);
        sMetric.set_span_error(s);
    }
    sMetric.stop(id);
    sMetric.set_span_tag(id, Jaeger::Metric::Tag{"result", "success"});
    sMetric.set_span_tag(id, Jaeger::Metric::Tag{"count", 50l});
    id = sMetric.start("commit");     std::this_thread::sleep_for(10us);  sMetric.stop(id);

    // serialize and send via UDP
    const std::string sMessage = sMetric.serialize();
    Udp::Producer sUdp("jaeger-agent.jaeger.docker", 6831); // jaeger agent : 6831
    sUdp.write(sMessage.data(), sMessage.size());
}
BOOST_AUTO_TEST_SUITE_END()