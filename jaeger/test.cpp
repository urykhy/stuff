#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include "Jaeger.hpp"
#include "unsorted/Uuid.hpp"
#include <networking/UdpPipe.hpp>

BOOST_AUTO_TEST_SUITE(Jaeger)
BOOST_AUTO_TEST_CASE(simple)
{
    using Tag = Jaeger::Metric::Tag;
    Jaeger::Metric sMetric("test.cpp", Util::Uuid64());
    sMetric.set_process_tag(Tag{"version","0.1/test"});
    size_t id = 0;
    id = sMetric.start("initialize"); std::this_thread::sleep_for(100us); sMetric.stop(id);
    id = sMetric.start("download");   std::this_thread::sleep_for(200us); sMetric.stop(id);
    id = sMetric.start("process");
    {
        size_t s = 0;
        s = sMetric.start("fetch", id); std::this_thread::sleep_for(200us); sMetric.stop(s);
        s = sMetric.start("merge", id); std::this_thread::sleep_for(300us); sMetric.stop(s);
        sMetric.span_log(s, Tag{"factor", 42.2}, Tag{"duplicates", 50l}, Tag{"unique", 10l}, Tag{"truncated", 4l});
        s = sMetric.start("write", id); std::this_thread::sleep_for(500us); sMetric.stop(s);
        sMetric.set_span_error(s);
    }
    sMetric.stop(id);
    sMetric.set_span_tag(id, Tag{"result", "success"});
    sMetric.set_span_tag(id, Tag{"count", 50l});
    id = sMetric.start("commit");     std::this_thread::sleep_for(10us);  sMetric.stop(id);

    // serialize and send via UDP
    const std::string sMessage = sMetric.serialize();
    Udp::Producer sUdp("jaeger-agent.jaeger.docker", 6831); // jaeger agent : 6831
    sUdp.write(sMessage.data(), sMessage.size());
}
BOOST_AUTO_TEST_SUITE_END()