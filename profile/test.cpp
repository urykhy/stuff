#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#define CATAPULT_PROFILE
#include "Catapult.hpp"
#include "Profile.hpp"

#include <threads/Group.hpp>
#include <time/Meter.hpp>

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
BOOST_AUTO_TEST_SUITE_END() // profile

BOOST_AUTO_TEST_SUITE(catapult)
BOOST_AUTO_TEST_CASE(simple)
{
    using namespace std::chrono_literals;

    Threads::Group sGroup;

    CATAPULT_MANAGER("/tmp/__profile.json")
    CATAPULT_START(sGroup);
    CATAPULT_THREAD("main")
    {
        CATAPULT_EVENT("step 1", "level 1");
        {
            std::this_thread::sleep_for(50ms);
            CATAPULT_EVENT("step 1", "level 2")
            std::this_thread::sleep_for(50ms);
            {
                std::this_thread::sleep_for(50ms);
                CATAPULT_EVENT("step 1", "level 3")
                CATAPULT_COUNTER("step 1", "time", ::time(nullptr))
                std::this_thread::sleep_for(50ms);
                std::this_thread::sleep_for(50ms);
            }
            std::this_thread::sleep_for(50ms);
        }
        {
            CATAPULT_EVENT("step 2", "wait thread")
            std::thread([&]() {
                CATAPULT_THREAD("thread");
                CATAPULT_EVENT("step 2", "work in thread")
                std::this_thread::sleep_for(50ms);
                CATAPULT_MARK("step 2", "in the middle")
                std::this_thread::sleep_for(50ms);
            }).join();
        }
    }
    CATAPULT_DONE()

    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END() // catapult