#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include <fmt/core.h>

#include <thread>

#include <boost/test/data/test_case.hpp>

#include "Balancer.hpp"
#include "Notify.hpp"
#include "NotifyWeight.hpp"

#include <parser/Atoi.hpp>

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

BOOST_FIXTURE_TEST_SUITE(etcd, WithClient)
BOOST_AUTO_TEST_CASE(simple)
{
    SD::Notify::Params sParams;
    sParams.key        = "notify/instance/a01";
    sParams.ttl        = 2;
    sParams.period     = 1;
    std::string sValue = R"({"latency": 0.1})";

    auto sNotify = std::make_shared<SD::Notify>(m_Asio.service(), sParams, sValue);
    sNotify->start();
    std::this_thread::sleep_for(4s); // sleep more than ttl

    // ensure key still here
    BOOST_CHECK_EQUAL(true, sNotify->status().empty());
    BOOST_CHECK_EQUAL(sValue, m_Client.get(sParams.key));

    // update value, it must be updated in etcd too
    sValue = R"({"latency": 0.1, "idle": 5})";
    sNotify->update(sValue);
    std::this_thread::sleep_for(2s);
    BOOST_CHECK_EQUAL(sValue, m_Client.get(sParams.key));

    // start balancer and get state
    SD::Balancer::Params sBalancerParams;
    sBalancerParams.prefix = "notify/instance/";
    auto sBalancer         = std::make_shared<SD::Balancer::Engine>(m_Asio.service(), sBalancerParams);
    sBalancer->start().get();

    const auto sState = sBalancer->state();
    BOOST_REQUIRE_EQUAL(1, sState.size());
    const auto& sEntry = *sState.begin();
    BOOST_CHECK_EQUAL("a01", sEntry.key);
    BOOST_CHECK_EQUAL(10, sEntry.weight);

    // pick
    BOOST_CHECK_EQUAL("a01", sBalancer->random().first->key());

    // stop notifier
    sNotify->stop();
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(100ms);
        if (sNotify->status() == "stopped")
            break;
    }

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
        sNotify.add(NOW + i / 10, 0.002);
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(0.2s);

    BOOST_TEST_MESSAGE("current weight: " << 1 / sNotify.info().latency);
    BOOST_TEST_MESSAGE("current rps: " << sNotify.info().rps);
    BOOST_CHECK_CLOSE(sNotify.info().rps, 10, 5 /* % */);
    const std::string sValue = m_Client.get(sParams.notify.key);
    BOOST_TEST_MESSAGE("etcd value: " << sValue);
    auto sRoot = Parser::Json::parse(sValue);
    BOOST_CHECK_CLOSE(sRoot["latency"].asDouble(), sNotify.info().latency, 5 /* % */);
}
BOOST_AUTO_TEST_SUITE_END()

struct WithBreaker : WithClient
{
    std::shared_ptr<SD::Balancer::Engine> m_Balancer;
    struct Stat
    {
        unsigned success = 0;
        unsigned fail    = 0;
    };
    std::map<std::string, Stat> m_Stat;
    static constexpr size_t     COUNT = 20000;

    WithBreaker(SD::Balancer::Params sParams = {})
    : m_Balancer(std::make_shared<SD::Balancer::Engine>(m_Asio.service(), sParams))
    {
    }

    void init_state()
    {
        std::vector<SD::Entry> sData{
            {.key = "a", .rps = 0.3, .weight = 1},
            {.key = "b", .rps = 0.3, .weight = 1},
            {.key = "c", .rps = 0.3, .weight = 1},
        };
        apply_weights(sData, [this]() {
            m_Balancer->m_State.m_Info[0]->reset(0, 1.0); // latency, success_rate
            m_Balancer->m_State.m_Info[1]->reset(0, 0.8); // reduced weight
            m_Balancer->m_State.m_Info[2]->reset(0, 0.4); // reduced weight + drops
        });
    }

    template <class T>
    void apply_weights(const std::vector<SD::Entry>& aData, T&& aTune)
    {
        // ajust and print weights
        auto sStep = [&]() {
            std::vector<SD::Entry> sTmpData{aData}; // force tmp copy
            m_Balancer->update(std::move(sTmpData));
            {
                auto sState = m_Balancer->state();
                for (auto& sPeer : sState)
                    BOOST_TEST_MESSAGE(fmt::format("{} | weight {:>4.2f}", sPeer.key, sPeer.weight));
            }
        };
        const int UPD_COUNT = 10;
        for (int i = 0; i < UPD_COUNT; i++) {
            sStep();
            if (i == 0)
                aTune();
        }
    }
    void put_load()
    {
        for (size_t i = 0; i < COUNT; i++) {
            try {
                auto sPeer = m_Balancer->random(i / 100).first->key();
                m_Stat[sPeer].success++;
            } catch (const SD::Balancer::Error& e) {
                std::string sTmp  = e.what();
                size_t      sPos  = sTmp.find(" blocked by");
                std::string sPeer = sTmp.substr(sPos - 1, 1); // one-char names
                m_Stat[sPeer].fail++;
            }
        }
    }
    void print_stats()
    {
        for (auto& x : m_Stat) {
            const double sSuccessPct = 100 * x.second.success / double(COUNT);
            const double sFailPct    = 100 * x.second.fail / double(COUNT);
            const double sTotalPct   = 100 * (x.second.fail + x.second.success) / double(COUNT);
            BOOST_TEST_MESSAGE(fmt::format("{} | total share ({:>4.1f}%) | success {:>5} ({:>4.1f}%) | fail {:>5} ({:>4.1f}%)",
                                           x.first, sTotalPct, x.second.success, sSuccessPct, x.second.fail, sFailPct));
        }
    }
};

BOOST_AUTO_TEST_SUITE(balancer)
BOOST_AUTO_TEST_CASE(ewma)
{
    Util::Ewma sSlow(0.95, 0.1); // start with 0.1 latency
    Util::Ewma sFast(0.95 * 0.75, 0.1);
    Util::EwmaRps sRPS(0.95, 0.1);

    for (int i = 0; i < 5; i++)
    {
        sSlow.add(0.2); // latency 0.2 now
        sFast.add(0.2);
        sRPS.add(i, 0.2, true);
    }
    BOOST_CHECK(sSlow.estimate() < sFast.estimate());
    BOOST_TEST_MESSAGE("estimate1: " << sSlow.estimate() << "/" << sFast.estimate() << "; ewma rps: " << sRPS.estimate().latency);

    for (int i = 5; i < 10; i++)
    {
        sSlow.add(0.1); // latency 0.1 again
        sFast.add(0.1);
        sRPS.add(i, 0.1, true);
    }
    BOOST_TEST_MESSAGE("estimate2: " << sSlow.estimate() << "/" << sFast.estimate() << "; ewma rps: " << sRPS.estimate().latency);
}
BOOST_FIXTURE_TEST_CASE(balance_by_weight, WithClient)
{
    SD::Balancer::Params sBalancerParams;
    sBalancerParams.prefix = "notify/instance/";
    auto sBalancer         = std::make_shared<SD::Balancer::Engine>(m_Asio.service(), sBalancerParams);

    const double           TOTALW = 1 + 3 + 6;
    std::vector<SD::Entry> sData{{.key = "a", .weight = 1}, {.key = "b", .weight = 3}, {.key = "c", .weight = 6}};
    sBalancer->update(std::move(sData));

    std::map<std::string, size_t> sStat;
    const size_t                  COUNT = 20000;
    for (size_t i = 0; i < COUNT; i++) {
        sStat[sBalancer->random().first->key()]++;
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
BOOST_AUTO_TEST_CASE(with_breaker)
{
    WithBreaker sFixture({.second_chance = false});
    sFixture.init_state();
    sFixture.put_load();
    sFixture.print_stats();

    BOOST_CHECK_EQUAL(3, sFixture.m_Stat.size());
    BOOST_CHECK_CLOSE(sFixture.m_Stat["c"].fail * 100 / double(WithBreaker::COUNT), 11., 5 /* % */);
}
BOOST_AUTO_TEST_CASE(with_breaker_second_chance)
{
    WithBreaker sFixture;
    sFixture.init_state();
    sFixture.put_load();
    sFixture.print_stats();

    BOOST_CHECK_EQUAL(3, sFixture.m_Stat.size());
    BOOST_CHECK_EQUAL(sFixture.m_Stat["c"].fail, 0.);
    BOOST_CHECK_CLOSE(sFixture.m_Stat["c"].success * 100 / double(WithBreaker::COUNT), 11., 5 /* % */);
}
BOOST_AUTO_TEST_CASE(with_client_latency)
{
    WithBreaker sFixture;

    std::vector<SD::Entry> sData{{.key = "a", .weight = 1}, {.key = "b", .weight = 1}, {.key = "c", .weight = 1}};
    sFixture.apply_weights(sData, [&sFixture]() {
        sFixture.m_Balancer->m_State.m_Info[0]->reset(1.0, 1.0); // latency, success_rate
        sFixture.m_Balancer->m_State.m_Info[1]->reset(1.5, 1.0);
        sFixture.m_Balancer->m_State.m_Info[2]->reset(2.0, 1.0);
    });

    // check
    auto sFinal = [&]() {
        std::map<std::string, double> sFinal;

        auto sState = sFixture.m_Balancer->state();
        for (auto& sPeer : sState)
            sFinal[sPeer.key] = sPeer.weight;
        return sFinal;
    }();

    BOOST_CHECK_CLOSE(sFinal["a"], 1., 1.);
    BOOST_CHECK_CLOSE(sFinal["b"], 0.67, 1.);
    BOOST_CHECK_CLOSE(sFinal["c"], 0.5, 1.);
}
BOOST_AUTO_TEST_CASE(with_session)
{
    WithBreaker            sFixture;
    std::vector<SD::Entry> sData{{.key = "a", .weight = 10}, {.key = "b", .weight = 10}, {.key = "c", .weight = 10}};
    sFixture.apply_weights(sData, [&sFixture]() {
        sFixture.m_Balancer->m_State.m_Info[0]->reset(0.0, 1.0); // latency, success_rate
        sFixture.m_Balancer->m_State.m_Info[1]->reset(0.0, 1.0);
        sFixture.m_Balancer->m_State.m_Info[2]->reset(0.0, 1.0);
    });

    // to place 10 calls to single peer, set minimal step to 1
    sFixture.m_Balancer->m_State.m_Step = 1;
    for (size_t i = 0; i < 10; i++) {
        auto sPair = sFixture.m_Balancer->random(1 /* timestamp */, 5 /* single session */);
        sPair.first->add(1 /* timestamp */, 0, true, sPair.second);
        auto sPeer = sPair.first->key();
        BOOST_CHECK_EQUAL("c", sPeer);
    }

    // log/check metrics
    uint32_t          sCalls  = 0;
    const auto        sActual = Prometheus::Manager::instance().toPrometheus();
    const std::string sVar    = "sd_call_count{peer=\"c\",kind=\"session\"} ";
    for (auto x : sActual) {
        if (x.find("peer=\"c\"") != std::string::npos) {
            BOOST_TEST_MESSAGE(x);
            if (String::starts_with(x, sVar))
                sCalls = Parser::Atoi<uint32_t>(x.substr(sVar.size()));
        }
    }
    BOOST_CHECK_EQUAL(10, sCalls);
}
BOOST_AUTO_TEST_SUITE_END()