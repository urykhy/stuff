#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <threads/Group.hpp>
#include <time/Meter.hpp>

#include "Profile.hpp"

static void sleep_load()
{
    Time::Deadline sDeadline(1.0);
    std::mutex     sMutex;
    auto           f = [&sMutex, sDeadline]() {
        while (!sDeadline.expired()) {
            std::unique_lock<std::mutex> sLock(sMutex);
            Threads::sleep(0.01);
        }
    };
    Threads::Group sGroup;
    sGroup.start(f, 4);
    sleep(1);
}

static void spin_load()
{
    static double  s = 0;
    Time::Deadline sDeadline(1.0);
    while (!sDeadline.expired())
        s += std::pow(M_E, M_PI);
}

BOOST_AUTO_TEST_SUITE(profile)

BOOST_AUTO_TEST_SUITE(off)
BOOST_AUTO_TEST_CASE(sleep)
{
    std::thread sProfiler([]() { Profile::Off("/tmp/flamegraph-off-sleep.svg", 1); });
    Threads::sleep(0.1);
    sleep_load();
    sProfiler.join();
}
BOOST_AUTO_TEST_CASE(spin)
{
    std::thread sProfiler([]() { Profile::Off("/tmp/flamegraph-off-spin.svg", 1); });
    Threads::sleep(0.1);
    spin_load();
    sProfiler.join();
}
BOOST_AUTO_TEST_SUITE_END() // offcpu

BOOST_AUTO_TEST_SUITE(cpu)
BOOST_AUTO_TEST_CASE(sleep)
{
    std::thread sProfiler([]() { Profile::CPU("/tmp/flamegraph-cpu-sleep.svg", 1, 1000); });
    Threads::sleep(0.1);
    sleep_load();
    sProfiler.join();
}
BOOST_AUTO_TEST_CASE(spin)
{
    std::thread sProfiler([]() { Profile::CPU("/tmp/flamegraph-cpu-spin.svg", 1, 1000); });
    Threads::sleep(0.1);
    spin_load();
    sProfiler.join();
}
BOOST_AUTO_TEST_SUITE_END() // cpu

BOOST_AUTO_TEST_SUITE_END() // profile
