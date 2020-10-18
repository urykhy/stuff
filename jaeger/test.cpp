#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include <thread>

#include <networking/Resolve.hpp>
#include <networking/UdpSocket.hpp>
#include <unsorted/Uuid.hpp>
#include "Jaeger.hpp"

BOOST_AUTO_TEST_SUITE(Jaeger)
BOOST_AUTO_TEST_CASE(simple)
{
    using Tag = Jaeger::Metric::Tag;
    Jaeger::Metric sMetric("test.cpp", Util::Uuid64(), 32);
    sMetric.set_process_tag(Tag{"version","0.1/test"});

    { Jaeger::Metric::Guard sM(sMetric, "initialize"); std::this_thread::sleep_for(100us); }
    { Jaeger::Metric::Guard sM(sMetric, "download"); std::this_thread::sleep_for(100us); }

    Jaeger::Metric::Guard sProcess(sMetric, "process");
    {
        { Jaeger::Metric::Guard sM(sProcess.child("fetch")); std::this_thread::sleep_for(200us); }
        { Jaeger::Metric::Guard sM(sProcess.child("merge")); std::this_thread::sleep_for(300us); sM.set_log(Tag{"factor", 42.2}, Tag{"duplicates", 50l}, Tag{"unique", 10l}, Tag{"truncated", 4l}); }
        try { Jaeger::Metric::Guard sM(sProcess.child("write")); std::this_thread::sleep_for(300us); throw 1; } catch(...) {}
    }
    sProcess.set_tag(Tag{"result", "success"});
    sProcess.set_tag(Tag{"count", 50l});
    sProcess.close();

    { Jaeger::Metric::Guard sM(sMetric, "commit"); std::this_thread::sleep_for(10us); }

    // serialize and send via UDP
    const std::string sMessage = sMetric.serialize();
    Udp::Socket sUdp(Util::resolveAddr("jaeger-agent.jaeger.docker"), 6831); // jaeger agent : 6831
    sUdp.write(sMessage.data(), sMessage.size());
}
BOOST_AUTO_TEST_SUITE_END()
