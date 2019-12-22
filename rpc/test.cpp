#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>

#include "RPC.hpp"
#include <threads/Asio.hpp>
#include <networking/Resolve.hpp>

BOOST_AUTO_TEST_SUITE(RPC)
BOOST_AUTO_TEST_CASE(library)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    RPC::Library sLib;
    sLib.insert("foo",[](auto s){ return s+"bar"; });
    sLib.insert("throw",[](auto s){ throw std::runtime_error("exception from call"); return "";});

    BOOST_CHECK_EQUAL(sLib.debug_call("foo","test").result(), "testbar");
    BOOST_CHECK_EQUAL(sLib.debug_call("bar","123").error(), "method not found");
    BOOST_CHECK_EQUAL(sLib.debug_call("throw","").error(), "exception from call");
}
BOOST_AUTO_TEST_CASE(simple)
{
    constexpr uint16_t PORT = 1234;
    using namespace std::chrono_literals;

    Util::EPoll sEpoll;
    Threads::Group sGroup;
    sEpoll.start(sGroup);

    RPC::Library sLibrary;
    sLibrary.insert("add_bar",[](auto& s){ return s + "bar"; });

    auto sServer = std::make_shared<RPC::Server>(PORT, sLibrary);
    sEpoll.post([sServer](Util::EPoll* ptr) { ptr->insert(sServer->get(), EPOLLIN, sServer); });

    auto sClient = std::make_shared<RPC::Client>(&sEpoll, Util::resolveName("127.0.0.1"), PORT);
    sEpoll.post([sClient](Util::EPoll* ptr) { ptr->insert(sClient->get(), EPOLLIN, sClient); });

    std::this_thread::sleep_for(10ms);

    uint32_t sCounter = 0;
    sClient->call("add_bar","foo",[&sCounter](auto&& sResult)
    {
        BOOST_CHECK_EQUAL(sResult.get(), "foobar");
        sCounter++;
    });
    std::this_thread::sleep_for(10ms); // wait for processing
    BOOST_CHECK_EQUAL(sCounter, 1);

    sGroup.wait();
}
#if 0
BOOST_AUTO_TEST_CASE(simple)
{
    constexpr uint16_t PORT = 1234;
    using namespace std::chrono_literals;

    std::atomic_uint sCount{0};
    Threads::Group sGroup;
    Threads::Asio  sLoop;
    sLoop.start(1, sGroup);

    auto sServer = std::make_shared<RPC::Server>(sLoop.service(), PORT);
    sServer->insert("foo",[](auto& s){
        using namespace std::chrono_literals;
        if (s == "with delay")
            std::this_thread::sleep_for(150ms);
        return s+"bar";
    });
    sServer->start();

    const auto sAddr = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT);
    auto sClient = std::make_shared<RPC::Client>(sLoop.service(), sAddr);
    sClient->start();

    // wait to get connection
    std::this_thread::sleep_for(200ms);

    // make calls, callback will be called in asio thread
    sClient->call("foo","bar",[&sCount](std::future<std::string>&& aResult){
        sCount++;
        BOOST_CHECK_EQUAL(aResult.get(), "barbar");
    });
    sClient->call("bar","xxx",[&sCount](std::future<std::string>&& aResult){
        sCount++;
        try {
            aResult.get();
            BOOST_CHECK_MESSAGE(false, "got result, but exception expected");
        } catch (const std::exception& e) {
            BOOST_CHECK_EQUAL(e.what(), "remote error: method not found");
        }
    });
    sClient->call("foo","with delay",[&sCount](std::future<std::string>&& aResult){
        sCount++;
        try {
            aResult.get();
            BOOST_CHECK_MESSAGE(false, "got result, but exception expected");
        } catch (const std::exception& e) {
            BOOST_CHECK_EQUAL(e.what(), "remote error: timeout");
        }
    });

    std::this_thread::sleep_for(200ms);
    sGroup.wait();
    BOOST_CHECK_EQUAL(sCount, 3);
}
#endif
BOOST_AUTO_TEST_SUITE_END()