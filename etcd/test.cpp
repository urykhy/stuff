#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;
#include <thread>

#include "Balancer.hpp"
#include "Etcd.hpp"
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

BOOST_FIXTURE_TEST_SUITE(etcd, WithClient)
BOOST_AUTO_TEST_CASE(simple)
{
    const std::string sKey = "alive/name";

    BOOST_CHECK_EQUAL(m_Client.get(sKey).size(), 0);
    m_Client.put(sKey, "1");
    BOOST_CHECK_EQUAL(m_Client.get(sKey), "1");

    auto sList = m_Client.list(sKey);
    BOOST_REQUIRE_EQUAL(1, sList.size());
    BOOST_CHECK_EQUAL(sList.front().key, sKey);
    BOOST_CHECK_EQUAL(sList.front().value, "1");

    m_Client.remove(sKey);
    BOOST_CHECK_EQUAL(m_Client.get(sKey), "");
}
BOOST_AUTO_TEST_CASE(atomic)
{
    const std::string sKey = "atomic/name";
    m_Client.atomicPut(sKey, "test");
    BOOST_CHECK_THROW(m_Client.atomicPut(sKey, "test"), Etcd::TxnError);

    m_Client.atomicUpdate(sKey, "test", "success");
    BOOST_CHECK_EQUAL(m_Client.get(sKey), "success");
    BOOST_CHECK_THROW(m_Client.atomicUpdate(sKey, "test", "success"), Etcd::TxnError);

    m_Client.atomicRemove(sKey, "success");
}
BOOST_AUTO_TEST_CASE(lease_keepalive)
{
    const std::string sKey = "alive/name";

    int64_t sLease = m_Client.createLease(2);
    m_Client.put(sKey, "1", sLease);
    for (int i = 0; i < 4; i++) {
        sleep(1);
        m_Client.updateLease(sLease);
    }
    BOOST_CHECK_EQUAL(m_Client.get(sKey), "1");
    sleep(3);
    BOOST_CHECK_EQUAL(m_Client.get(sKey), "");
}
BOOST_AUTO_TEST_CASE(lease_drop)
{
    const std::string sKey = "alive/name";

    int64_t sLease = m_Client.createLease(2);
    m_Client.put(sKey, "2", sLease);
    BOOST_CHECK_EQUAL(m_Client.get(sKey), "2");
    sleep(1);
    m_Client.dropLease(sLease);
    BOOST_CHECK_EQUAL(m_Client.get(sKey), "");
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(notify, WithClient)
BOOST_AUTO_TEST_CASE(simple)
{
    Etcd::Notify::Params sParams;
    sParams.key        = "notify/instance/a01";
    sParams.ttl        = 2;
    sParams.period     = 1;
    std::string sValue = R"({"weight": 10})";

    auto sNotify = std::make_shared<Etcd::Notify>(m_Asio.service(), sParams, sValue);
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
    Etcd::Balancer::Params sBalancerParams;
    sBalancerParams.prefix = "notify/instance/";
    auto sBalancer = std::make_shared<Etcd::Balancer>(m_Asio.service(), sBalancerParams);
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
