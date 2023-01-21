#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include <thread>

#include <boost/test/data/test_case.hpp>

#include "Balancer.hpp"
#include "Breaker.hpp"
#include "Notify.hpp"
#include "NotifyWeight.hpp"

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

BOOST_FIXTURE_TEST_SUITE(sd, WithClient)
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
    const auto& sEntry = sState.begin()->second;
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
BOOST_AUTO_TEST_CASE(notify_weight)
{
    SD::NotifyWeight::Params sParams;
    sParams.notify.key = "notify/instance/a01";
    SD::NotifyWeight sNotify(m_Asio.service(), sParams);

    const time_t NOW   = time(nullptr);
    const int    COUNT = 2000;
    for (int i = 0; i < COUNT; i++) {
        sNotify.add(0.002, NOW + i / 10);
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(0.2s);

    BOOST_TEST_MESSAGE("current weight: " << sNotify.weight());
    const std::string sValue = m_Client.get(sParams.notify.key);
    BOOST_TEST_MESSAGE("etcd value: " << sValue);
    auto sRoot = Parser::Json::parse(sValue);
    BOOST_CHECK_CLOSE(sRoot["weight"].asDouble(), sNotify.weight(), 5 /* % */);
}
BOOST_AUTO_TEST_CASE(balance_by_weight)
{
    SD::Balancer::Params sBalancerParams;
    sBalancerParams.prefix = "notify/instance/";
    auto sBalancer         = std::make_shared<SD::Balancer>(m_Asio.service(), sBalancerParams);

    const double                     TOTALW = 1 + 3 + 6;
    std::vector<SD::Balancer::Entry> sData{{"a", 1, ""}, {"b", 3, ""}, {"c", 6, ""}};
    sBalancer->update(sData);

    std::map<std::string, size_t> sStat;
    const size_t                  COUNT = 20000;
    for (size_t i = 0; i < COUNT; i++) {
        sStat[sBalancer->random().key]++;
    }
    for (auto& x : sStat) {
        if (x.first == "a")
            BOOST_CHECK_CLOSE(1 / TOTALW, x.second / (double)COUNT, 5);
        else if (x.first == "b")
            BOOST_CHECK_CLOSE(3 / TOTALW, x.second / (double)COUNT, 5);
        else if (x.first == "c")
            BOOST_CHECK_CLOSE(6 / TOTALW, x.second / (double)COUNT, 5);
    }
}
BOOST_AUTO_TEST_CASE(balance_with_breaker)
{
    // prepare breaker state
    auto sBreaker = std::make_shared<SD::Breaker>("not-used");
    sBreaker->reset("a", 1.0, 0);
    sBreaker->reset("b", 0.8, 0); // reduced weight
    sBreaker->reset("c", 0.6, 0); // reduced weight + drops

    // prepare balancer and link to breaker
    SD::Balancer::Params sBalancerParams;
    auto                 sBalancer = std::make_shared<SD::Balancer>(m_Asio.service(), sBalancerParams);
    sBalancer->with_peer_stat(sBreaker); // use breaker success rate
    std::vector<SD::Balancer::Entry> sData{{"a", 1, ""}, {"b", 1, ""}, {"c", 1, ""}};
    sBalancer->update(sData);

    // balancer state
    {
        auto sState = sBalancer->state();
        for (auto& [_, sPeer] : sState)
            BOOST_TEST_MESSAGE(fmt::format("{} | weight {:>4.2f}", sPeer.key, sPeer.weight));
    }

    // put load
    struct Stat
    {
        unsigned success = 0;
        unsigned fail    = 0;
    };
    std::map<std::string, Stat> sStat;
    const size_t                COUNT = 20000;
    for (size_t i = 0; i < COUNT; i++) {
        const std::string sPeer = sBalancer->random().key;
        if (sBreaker->test(sPeer))
            sStat[sPeer].success++;
        else
            sStat[sPeer].fail++;
    }

    // output stats
    for (auto& x : sStat) {
        const double sSuccessPct = 100 * x.second.success / double(COUNT);
        const double sFailPct    = 100 * x.second.fail / double(COUNT);
        const double sTotalPct   = 100 * (x.second.fail + x.second.success) / double(COUNT);
        BOOST_TEST_MESSAGE(fmt::format("{} | total share ({:>4.1f}%) | success {:>5} ({:>4.1f}%) | fail {:>5} ({:>4.1f}%)",
                                       x.first, sTotalPct, x.second.success, sSuccessPct, x.second.fail, sFailPct));
        if (x.first == "c")
            BOOST_CHECK_CLOSE(sFailPct, 10, 10);
    }
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(peer_state)
BOOST_DATA_TEST_CASE(constant_probability,
                     std::vector<double>({0, 0.25, 0.5, 0.60, 0.75, 1}),
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
                sStat.add(0, sTimestamp, Util::drand48() < sProb);
            }
        }
    }
    const double sActualRate = sAllowed / (double)(RPS * SECONDS);

    if (abs(sProb - 0) < 0.01)
        BOOST_CHECK_CLOSE(0.12, sActualRate, 15);
    else if (abs(sProb - 0.25) < 0.01)
        BOOST_CHECK_CLOSE(0.135, sActualRate, 15);
    else if (abs(sProb - 0.5) < 0.01)
        BOOST_CHECK_CLOSE(0.23, sActualRate, 25);
    else if (abs(sProb - 0.75) < 0.01)
        BOOST_CHECK_CLOSE(0.9, sActualRate, 25);
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
    BOOST_TEST_MESSAGE("* only successfull calls");
    for (auto i = 0; i < 10; i++) {
        sEwma.add(1);
        BOOST_TEST_MESSAGE("estimation: " << sEwma.estimate());
    }
}
BOOST_AUTO_TEST_SUITE_END()