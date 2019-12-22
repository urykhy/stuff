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
BOOST_AUTO_TEST_SUITE_END()