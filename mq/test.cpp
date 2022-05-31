#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include "Client.hpp"
#include "Server.hpp"

#include <asio_http/Server.hpp>

BOOST_AUTO_TEST_SUITE(mq)
BOOST_AUTO_TEST_CASE(simple)
{
    auto sRouter = std::make_shared<asio_http::Router>();

    MQ::Server::Params sParams;
    sParams.etcd.prefix      = "test:mq:";
    unsigned   sMessageCount = 0;
    MQ::Server sMQ(sParams, [&sMessageCount](std::string& aMessage) {
        sMessageCount++;
        if (sMessageCount == 1)
            BOOST_CHECK_EQUAL("qwerty123", aMessage);
        if (sMessageCount == 2)
            BOOST_CHECK_EQUAL("message123", aMessage);
        if (sMessageCount == 3)
            BOOST_CHECK_EQUAL("abc", aMessage);
        if (sMessageCount == 4)
            BOOST_CHECK_EQUAL("cde", aMessage);
    });
    sMQ.configure(sRouter);

    Threads::Asio sAsio;
    asio_http::startServer(sAsio.service(), 2081, sRouter);

    // client part
    MQ::Client::Params sClientParams;
    sClientParams.url = "http://localhost:2081/mq";
    MQ::Client sClient(sClientParams);

    Threads::Group sGroup;
    sAsio.start(sGroup);
    sMQ.start(sGroup);
    std::this_thread::sleep_for(10ms);

    // put block
    auto sResponse = sClient.put("1:12345", "qwerty123");
    BOOST_CHECK_EQUAL(sResponse, 200);

    // put next block
    sResponse = sClient.put("1:123456", "message123");
    BOOST_CHECK_EQUAL(sResponse, 200);

    // put duplicate
    sResponse = sClient.put("1:12345", "qwerty123");
    BOOST_CHECK_EQUAL(sResponse, 200);
    BOOST_CHECK_EQUAL(sMessageCount, 2);

    sClient.start(sGroup);

    sClient.insert("2:abc", "abc");
    sClient.insert("2:abc", "abc");
    sClient.insert("3:cde", "cde");

    Threads::sleep(0.1);
    BOOST_CHECK_EQUAL(sMessageCount, 4);

    // push some more blocks and test if old hashes removed from state
    sClient.insert("3:cde1", "cde");
    sClient.insert("3:cde2", "cde");
    sClient.insert("3:cde3", "cde");
    sClient.insert("3:cde4", "cde");
    sClient.insert("3:cde5", "cde");
    sClient.insert("3:cde6", "cde"); // 10th block
    sClient.insert("3:cde7", "cde");
    sClient.insert("3:cde8", "cde");
    Threads::sleep(0.1);

    {
        Threads::Asio  sAsio;
        Threads::Group sGroup;
        sAsio.start(sGroup);

        Etcd::Client::Params sParams;
        Etcd::Client         sEtcd(sAsio.service(), sParams);
        const auto           sState = sEtcd.get("mq:default");

        // ensure 10 last hashes in history
        Json::Value sJson = Parser::Json::parse(sState);
        BOOST_REQUIRE_EQUAL(sJson.isArray(), true);
        BOOST_REQUIRE_EQUAL(sJson.size(), 10);
        BOOST_REQUIRE_EQUAL(sJson[0].asString(), "2:abc");

        // clear etcd state
        sEtcd.remove("mq:", true);
    }
}
BOOST_AUTO_TEST_SUITE_END()
