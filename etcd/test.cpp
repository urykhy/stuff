#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;
#include <thread>

#include "Balancer.hpp"
#include "Etcd.hpp"
#include "Notify.hpp"

Etcd::Client::Params sParams;
Etcd::Client         sClient(sParams);

BOOST_AUTO_TEST_SUITE(etcd)
BOOST_AUTO_TEST_CASE(simple)
{
    const std::string sKey = "alive/name";

    BOOST_CHECK_EQUAL(sClient.get(sKey).size(), 0);
    sClient.put(sKey, "1");
    BOOST_CHECK_EQUAL(sClient.get(sKey), "1");

    auto sList = sClient.list(sKey);
    BOOST_REQUIRE_EQUAL(1, sList.size());
    BOOST_CHECK_EQUAL(sList.front().key, sKey);
    BOOST_CHECK_EQUAL(sList.front().value, "1");

    sClient.remove(sKey);
    BOOST_CHECK_EQUAL(sClient.get(sKey), "");
}
BOOST_AUTO_TEST_SUITE(ttl)
BOOST_AUTO_TEST_CASE(keepalive)
{
    const std::string sKey = "alive/name";

    int64_t sLease = sClient.createLease(2);
    sClient.put(sKey, "1", sLease);
    for (int i = 0; i < 4; i++) {
        sleep(1);
        sClient.updateLease(sLease);
    }
    BOOST_CHECK_EQUAL(sClient.get(sKey), "1");
    sleep(3);
    BOOST_CHECK_EQUAL(sClient.get(sKey), "");
}
BOOST_AUTO_TEST_CASE(drop)
{
    const std::string sKey = "alive/name";

    int64_t sLease = sClient.createLease(2);
    sClient.put(sKey, "2", sLease);
    BOOST_CHECK_EQUAL(sClient.get(sKey), "2");
    sleep(1);
    sClient.dropLease(sLease);
    BOOST_CHECK_EQUAL(sClient.get(sKey), "");
}
BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_CASE(notify)
{
    Etcd::Notify::Params sParams;
    sParams.key    = "notify/instance/a01";
    sParams.ttl    = 2;
    sParams.period = 1;
    Etcd::Client sClient(sParams.addr);
    std::string  sValue = R"({"weight": 10})";

    Etcd::Notify   sNotify(sParams, sValue);
    Threads::Group sGroup;
    sNotify.start(sGroup);
    std::this_thread::sleep_for(4s); // sleep more than ttl

    // ensure key still here
    BOOST_CHECK_EQUAL(sValue, sClient.get(sParams.key));

    // update value, it must be updated in etcd too
    sValue = R"({"weight": 10, "idle": 5})";
    sNotify.update(sValue);
    std::this_thread::sleep_for(2s);
    BOOST_CHECK_EQUAL(sValue, sClient.get(sParams.key));

    // start balancer and get state
    Etcd::Balancer::Params sBalancerParams;
    sBalancerParams.prefix = "notify/instance/";
    Etcd::Balancer sBalancer(sBalancerParams);
    sBalancer.start(sGroup);

    std::this_thread::sleep_for(10ms);
    const auto sState = sBalancer.state();

    BOOST_REQUIRE_EQUAL(1, sState.size());
    const auto& sEntry = sState.front();
    BOOST_CHECK_EQUAL("a01", sEntry.key);
    BOOST_CHECK_EQUAL(10, sEntry.weight);

    // pick
    BOOST_CHECK_EQUAL("a01", sBalancer.random().key);

    // stop threads..
    sGroup.wait();
    // ensure key deleted from etcd
    BOOST_CHECK_EQUAL("", sClient.get(sParams.key));
}
BOOST_AUTO_TEST_CASE(atomic)
{
    const std::string sKey = "atomic/name";
    sClient.atomicPut(sKey, "test");
    BOOST_CHECK_THROW(sClient.atomicPut(sKey, "test"), Etcd::TxnError);

    sClient.atomicUpdate(sKey, "test", "success");
    BOOST_CHECK_EQUAL(sClient.get(sKey), "success");
    BOOST_CHECK_THROW(sClient.atomicUpdate(sKey, "test", "success"), Etcd::TxnError);

    sClient.remove(sKey);
}
BOOST_AUTO_TEST_SUITE_END()
