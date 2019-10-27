#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>

#include "RPC.hpp"
#include <threads/Asio.hpp>

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

    Threads::Group sGroup;
    Threads::Asio  sLoop;
    sLoop.start(1, sGroup);

    RPC::Server sServer(sLoop.service(), PORT);
    sServer.insert("foo",[](auto& s){ return s+"bar"; });

    const auto sAddr = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), PORT);
    RPC::Client sClient(sLoop.service(), sAddr);

    // wait to get connection
    std::this_thread::sleep_for(200ms);

    // make calls, callback will be called in asio thread
    sClient.call("foo","bar",[](std::future<std::string>&& aResult){
        BOOST_CHECK_EQUAL(aResult.get(), "barbar");
    });
    sClient.call("bar","xxx",[](std::future<std::string>&& aResult){
        try {
            aResult.get();
            BOOST_CHECK_MESSAGE(false, "got result, but exception expected");
        } catch (const std::exception& e) {
            BOOST_CHECK_EQUAL(e.what(), "method not found");
        }
    });

    std::this_thread::sleep_for(100ms);
    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()