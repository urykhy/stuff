#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;
#include <thread>

#include "Balancer.hpp"
#include "Etcd.hpp"
#include "Notify.hpp"

BOOST_AUTO_TEST_SUITE(etcd)
BOOST_AUTO_TEST_CASE(simple)
{
    Etcd::Client::Params sParams; // default is 127.0.0.1:2379 and prefix is test
    Etcd::Client         sClient(sParams);
    const std::string    sKey = "alive/name";

    sClient.set(sKey, "1");
    BOOST_CHECK_EQUAL("1", sClient.get(sKey).value);
    sClient.remove(sKey);
    BOOST_CHECK_EQUAL("", sClient.get(sKey).value);
}
BOOST_AUTO_TEST_CASE(ttl)
{
    Etcd::Client::Params sParams;
    Etcd::Client         sClient(sParams);
    const std::string    sKey = "alive/ttl";

    sClient.set(sKey, "1", 1);
    BOOST_CHECK_EQUAL("1", sClient.get(sKey).value);
    std::this_thread::sleep_for(2s);
    BOOST_CHECK_EQUAL("", sClient.get(sKey).value);
}
BOOST_AUTO_TEST_CASE(refresh)
{
    Etcd::Client::Params sParams;
    Etcd::Client         sClient(sParams);
    const std::string    sKey = "alive/refresh";

    sClient.set(sKey, "1", 1);
    for (int i = 0; i < 5; i++) {
        sClient.refresh(sKey, 1);
        std::this_thread::sleep_for(1s);
    }
    BOOST_CHECK_EQUAL("1", sClient.get(sKey).value);
}
BOOST_AUTO_TEST_CASE(cas)
{
    Etcd::Client::Params sParams;
    Etcd::Client         sClient(sParams);
    const std::string    sKey = "alive/cas";

    sClient.cas(sKey, "", "1");
    BOOST_CHECK_THROW(sClient.cas(sKey, "2", "3"), Etcd::Client::Error);
    sClient.cas(sKey, "1", "2");
    BOOST_CHECK_EQUAL("2", sClient.get(sKey).value);
    sClient.cad(sKey, "2");
    BOOST_CHECK_EQUAL("", sClient.get(sKey).value);
}
BOOST_AUTO_TEST_CASE(list)
{
    Etcd::Client::Params sParams;
    Etcd::Client         sClient(sParams);
    const std::string    sKey = "alive/list";

    sClient.set(sKey + "/one", "1");
    sClient.set(sKey + "/two", "2");
    sClient.set(sKey + "/three/or/not", "3");

    auto sList = sClient.list(sKey);
    BOOST_REQUIRE_EQUAL(3, sList.size());

    for (unsigned i = 0; i < sList.size(); i++) {
        auto&& x = sList[i];
        switch (i) {
        case 0:
            BOOST_CHECK_EQUAL("one", x.key);
            BOOST_CHECK_EQUAL(false, x.is_dir);
            break;
        case 1:
            BOOST_CHECK_EQUAL("three", x.key);
            BOOST_CHECK_EQUAL(true, x.is_dir);
            break;
        case 2:
            BOOST_CHECK_EQUAL("two", x.key);
            BOOST_CHECK_EQUAL(false, x.is_dir);
            break;
        }
    }
    sClient.rmdir(sKey);
}
BOOST_AUTO_TEST_CASE(enqueue)
{
    Etcd::Client::Params sParams;
    Etcd::Client         sClient(sParams);
    const std::string    sKey = "alive/dir";

    sClient.mkdir(sKey);
    sClient.enqueue(sKey, "job1");
    sClient.enqueue(sKey, "job2");
    sClient.enqueue(sKey, "job3");
    auto sList = sClient.list(sKey);
    BOOST_CHECK_EQUAL(3, sList.size());

    std::string sResult;
    for (unsigned i = 0; i < sList.size(); i++) {
        const std::string sItem = sKey + "/" + sList[i].key;
        BOOST_TEST_MESSAGE("process task " << sList[i].key);
        sResult += sClient.get(sItem).value;
        sClient.remove(sItem);
    }
    sClient.rmdir(sKey);
    BOOST_CHECK_EQUAL("job1job2job3", sResult);
}
BOOST_AUTO_TEST_CASE(notify)
{
    Etcd::Notify::Params sParams;
    sParams.key    = "notify/instance/a01";
    sParams.ttl    = 2;
    sParams.period = 1;
    Etcd::Client sClient(sParams);
    std::string  sValue = R"({"weight": 10})";

    Etcd::Notify   sNotify(sParams, sValue);
    Threads::Group sGroup;
    sNotify.start(sGroup);
    std::this_thread::sleep_for(4s); // sleep more than ttl

    // ensure key still here
    BOOST_CHECK_EQUAL(sValue, sClient.get(sParams.key).value);

    // update value, it must be updated in etcd too
    sValue = R"({"weight": 10, "idle": 5})";
    sNotify.update(sValue);
    std::this_thread::sleep_for(2s);
    BOOST_CHECK_EQUAL(sValue, sClient.get(sParams.key).value);

    // start balancer and get state
    Etcd::Balancer::Params sBalancerParams;
    sBalancerParams.key = "notify/instance";
    Etcd::Balancer sBalancer(sBalancerParams);
    sBalancer.start(sGroup);

    std::this_thread::sleep_for(10ms);
    const auto sState = sBalancer.state();

    BOOST_CHECK_EQUAL(1, sState.size());
    const auto& sEntry = sState.front();
    BOOST_CHECK_EQUAL("a01", sEntry.key);
    BOOST_CHECK_EQUAL(10, sEntry.weight);

    // pick
    BOOST_CHECK_EQUAL("a01", sBalancer.random().key);

    // stop threads..
    sGroup.wait();
    // ensure key deleted from etcd
    BOOST_CHECK_EQUAL("", sClient.get(sParams.key).value);
}
BOOST_AUTO_TEST_SUITE_END()
