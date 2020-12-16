#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <vector>

#include <time/Meter.hpp>

#define CL_TARGET_OPENCL_VERSION 120
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#include <boost/compute.hpp>
#pragma GCC diagnostic pop

namespace compute = boost::compute;

BOOST_AUTO_TEST_SUITE(opencl)
BOOST_AUTO_TEST_CASE(sort)
{
    // copy-paste from https://github.com/boostorg/compute # Example
    std::vector<float> sHost(1000000);
    std::generate(sHost.begin(), sHost.end(), rand);

    {
        auto        sTmp = sHost;
        Time::Meter sMeter;
        std::sort(sTmp.begin(), sTmp.end());
        BOOST_TEST_MESSAGE("cpu sort: " << sMeter.get() << " seconds");
        BOOST_CHECK(std::is_sorted(sTmp.begin(), sTmp.end()));
    }

    {
        compute::device        sGPU = compute::system::default_device();
        compute::context       sCTX(sGPU);
        compute::command_queue sQueue(sCTX, sGPU);
        BOOST_TEST_MESSAGE("using gpu: " << sGPU.name() << " with " << sGPU.compute_units() << " compute units, version: " << sGPU.version());

        Time::Meter sMeter;
        compute::sort(sHost.begin(), sHost.end(), sQueue);
        BOOST_TEST_MESSAGE("gpu sort: " << sMeter.get() << " seconds");
        BOOST_CHECK(std::is_sorted(sHost.begin(), sHost.end()));
    }
}
BOOST_AUTO_TEST_SUITE_END()
