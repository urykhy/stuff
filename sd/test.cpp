#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include <thread>

#include <boost/test/data/test_case.hpp>

#include "Balancer.hpp"
#include "Breaker.hpp"
#include "Notify.hpp"

struct WithClient
{
    Threads::Asio        m_Asio;
    Etcd::Client::Params m_Params;
    Etcd::Client         m_Client;
    Threads::Group       m_Group;

    WithClient()
    : m_Client(m_Asio.service(), m_Params)
    {
        m_Asio.start(m_Group);
    }
};

BOOST_FIXTURE_TEST_SUITE(notify, WithClient)
BOOST_AUTO_TEST_CASE(simple)
{
    SD::Notify::Params sParams;
    sParams.key        = "notify/instance/a01";
    sParams.ttl        = 2;
    sParams.period     = 1;
    std::string sValue = R"({"weight": 10})";

    auto sNotify = std::make_shared<SD::Notify>(m_Asio.service(), sParams, sValue);
    sNotify->start();
    std::this_thread::sleep_for(4s); // sleep more than ttl

    // ensure key still here
    BOOST_CHECK_EQUAL(true, sNotify->status().first);
    BOOST_CHECK_EQUAL(sValue, m_Client.get(sParams.key));

    // update value, it must be updated in etcd too
    sValue = R"({"weight": 10, "idle": 5})";
    sNotify->update(sValue);
    std::this_thread::sleep_for(2s);
    BOOST_CHECK_EQUAL(sValue, m_Client.get(sParams.key));

    // start balancer and get state
    SD::Balancer::Params sBalancerParams;
    sBalancerParams.prefix = "notify/instance/";
    auto sBalancer         = std::make_shared<SD::Balancer>(m_Asio.service(), sBalancerParams);
    sBalancer->start();

    std::this_thread::sleep_for(10ms);
    const auto sState = sBalancer->state();

    BOOST_REQUIRE_EQUAL(1, sState.size());
    const auto& sEntry = sState.front();
    BOOST_CHECK_EQUAL("a01", sEntry.key);
    BOOST_CHECK_EQUAL(10, sEntry.weight);

    // pick
    BOOST_CHECK_EQUAL("a01", sBalancer->random().key);

    // stop notifier
    sNotify->stop();
    std::this_thread::sleep_for(100ms);

    // ensure key deleted from etcd
    BOOST_CHECK_EQUAL("", m_Client.get(sParams.key));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(breaker)
BOOST_AUTO_TEST_CASE(start_wo_check)
{
    SD::PeerState sStat;
    BOOST_CHECK_CLOSE(sStat.success_rate(), 0.75, 1);
    const uint32_t COUNT = 50;
    for (uint32_t i = 1; i < COUNT; i++) {
        sStat.insert(i, true);
        BOOST_TEST_MESSAGE("after one more successful call: " << sStat.success_rate());
    }
    BOOST_CHECK_CLOSE(sStat.success_rate(), 1, 5);
}
BOOST_AUTO_TEST_CASE(start_with_check)
{
    SD::PeerState sStat;
    BOOST_CHECK_CLOSE(sStat.success_rate(), 0.75, 1);
    const uint32_t COUNT    = 50;
    uint32_t       sBlocked = 0;
    for (uint32_t i = 1; i < COUNT; i++) {
        if (sStat.test(i))
            sStat.insert(i, true);
        else
            sBlocked++;
        BOOST_TEST_MESSAGE("after one second: " << sStat.success_rate());
    }
    BOOST_CHECK_CLOSE(sStat.success_rate(), 1, 5);
    BOOST_TEST_MESSAGE("blocked calls: " << sBlocked);
}

BOOST_DATA_TEST_CASE(constant_probability,
                     std::vector<double>({0, 0.25, 0.5, 0.60, 0.75, 0.95, 1}),
                     sProb)
{
    constexpr unsigned SECONDS  = 500;
    constexpr unsigned RPS      = 100;
    unsigned           sAllowed = 0;
    SD::PeerState      sStat;

    for (unsigned sTimestamp = 0; sTimestamp < SECONDS; sTimestamp++) {
        for (unsigned i = 0; i < RPS; i++) {
            if (sStat.test(sTimestamp)) {
                sAllowed++;
                sStat.insert(sTimestamp, Util::drand48() < sProb);
            }
        }
    }
    const double sActualRate = sAllowed / (double)(RPS * SECONDS);

    if (abs(sProb - 0) < 0.01)
        BOOST_CHECK_CLOSE(0.1, sActualRate, 15);
    else if (abs(sProb - 0.25) < 0.01)
        BOOST_CHECK_CLOSE(0.135, sActualRate, 15);
    else if (abs(sProb - 0.5) < 0.01)
        BOOST_CHECK_CLOSE(0.23, sActualRate, 25);
    else
        BOOST_CHECK_CLOSE(sProb, sActualRate, 5);
}
BOOST_AUTO_TEST_CASE(ewma)
{
    Util::Ewma sEwma(SD::PeerState::EWMA_FACTOR, SD::PeerState::INITIAL_RATE);
    BOOST_TEST_MESSAGE("* only successfull calls");
    for (auto i = 0; i < 10; i++) {
        sEwma.add(1);
        BOOST_TEST_MESSAGE("estimation: " << sEwma.estimate());
    }
    BOOST_TEST_MESSAGE("* degradate slowly");
    for (auto i = 0; i < 40; i++) {
        sEwma.add(1 - i / 100.0);
        BOOST_TEST_MESSAGE("estimation: " << sEwma.estimate());
    }
    BOOST_TEST_MESSAGE("* recovering");
    for (auto i = 40; i > 0; i--) {
        sEwma.add(1 - i / 100.0);
        BOOST_TEST_MESSAGE("estimation: " << sEwma.estimate());
    }
}
BOOST_AUTO_TEST_SUITE_END()