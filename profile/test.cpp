#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

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

BOOST_AUTO_TEST_SUITE(catapult)
BOOST_AUTO_TEST_CASE(simple)
{
    using namespace std::chrono_literals;

    Profile::Catapult::Manager sManager("/tmp/__profile.json");
    {
        auto sData = sManager.start("step 1", "first level");
        {
            std::this_thread::sleep_for(50ms);
            auto sData = sManager.start("step 1", "second level");
            std::this_thread::sleep_for(50ms);
            {
                std::this_thread::sleep_for(50ms);
                auto sData = sManager.start("step 1", "third level");
                sManager.counter("step 1", "time", ::time(nullptr));
                std::this_thread::sleep_for(50ms);

                std::this_thread::sleep_for(50ms);
            }
            std::this_thread::sleep_for(50ms);
        }
        {
            auto sData = sManager.start("step 2", "start thread");
            std::thread([&]() {
                auto sData = sManager.start("step 2", "just wait");
                std::this_thread::sleep_for(50ms);
                sManager.instant("step 2", "in the middle");
                std::this_thread::sleep_for(50ms);
            }).join();
        }
/*
        {
            sManager.async_start("async", "test-a", "id");
            sManager.async_start("async", "test-b", "id");
            std::this_thread::sleep_for(50ms);
            sManager.async_stop("async", "test-a", "id");
            sManager.async_stop("async", "test-b", "id");
        }
*/
    }
    sManager.dump();
}
BOOST_AUTO_TEST_SUITE_END() // catapult