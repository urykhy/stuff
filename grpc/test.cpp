#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <thread>

#include <boost/asio/use_future.hpp>

#include "PlayGRPC.hpp"

#include <threads/Asio.hpp>
#include <unsorted/Process.hpp>

using namespace std::chrono_literals;

void GhzBench(const std::string& aAddr)
{
    auto sChild = Util::Perform("unbuffer", "ghz", "--insecure", "--async", "--proto", "../Play.proto", "--call", "play.PlayService/Ping",
                                "-c", "10", "-n", "60000", "--rps", "30000", "-d",
                                R"({"value":"{{.RequestNumber}}"})", aAddr);
    BOOST_CHECK_EQUAL(0, sChild.code);
    BOOST_TEST_MESSAGE(sChild.out);
}

BOOST_AUTO_TEST_SUITE(GRPC)
BOOST_AUTO_TEST_CASE(basic)
{
    const std::string sAddr = "127.0.0.1:56780";
    PlayGRPC::Server  sServer;
    PlayGRPC::Client  sClient(sAddr);

    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);

    BOOST_CHECK_EQUAL(123, sClient.Ping(123));
}
BOOST_AUTO_TEST_CASE(ghz)
{
    const std::string sAddr = "127.0.0.1:56780";
    PlayGRPC::Server  sServer;
    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);
    GhzBench(sAddr);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Async)
BOOST_AUTO_TEST_CASE(basic)
{
    Threads::Asio  sAsio;
    Threads::Group sGroup;
    sAsio.start(sGroup);

    const std::string     sAddr = "127.0.0.1:56780";
    PlayGRPC::AsyncServer sServer(sAsio.service());
    PlayGRPC::Client      sClient(sAddr);

    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);

    BOOST_CHECK_EQUAL(123, sClient.Ping(123));
}
BOOST_AUTO_TEST_CASE(ghz)
{
    Threads::Asio  sAsio;
    Threads::Group sGroup;
    sAsio.start(sGroup);

    const std::string     sAddr = "127.0.0.1:56780";
    PlayGRPC::AsyncServer sServer(sAsio.service());
    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);
    GhzBench(sAddr);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Tradias)
BOOST_AUTO_TEST_CASE(basic)
{
    const std::string       sAddr = "127.0.0.1:56780";
    PlayGRPC::TradiasServer sServer;
    PlayGRPC::Client        sClient(sAddr);

    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);

    BOOST_CHECK_EQUAL(123, sClient.Ping(123));
}
BOOST_AUTO_TEST_CASE(client)
{
    const std::string       sAddr = "127.0.0.1:56780";
    PlayGRPC::TradiasServer sServer;
    PlayGRPC::TradiasClient sClient(sAddr);

    sServer.Start(sAddr);
    sClient.Start();
    std::this_thread::sleep_for(10ms);

    auto sFuture = boost::asio::co_spawn(
        sClient.Context(),
        [&]() -> boost::asio::awaitable<int> { co_return co_await sClient.Ping(123); },
        boost::asio::use_future);
    BOOST_CHECK_EQUAL(123, sFuture.get());
}
BOOST_AUTO_TEST_CASE(ghz)
{
    const std::string       sAddr = "127.0.0.1:56780";
    PlayGRPC::TradiasServer sServer;
    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);
    GhzBench(sAddr);
}
BOOST_AUTO_TEST_SUITE_END()
