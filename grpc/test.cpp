#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <thread>

#include <boost/asio/use_future.hpp>

#include "Play-generated.hpp"
#include "PlayGRPC.hpp"

#include <threads/Asio.hpp>
#include <unsorted/Process.hpp>

using namespace std::chrono_literals;

void GhzBench(const std::string& aAddr)
{
    putenv("GOMAXPROCS=1");
    auto sChild = Util::Perform("unbuffer", "ghz", "--insecure", "--async", "--proto", "../Play.proto", "--call", "play.PlayService/Ping",
                                "-c", "10", "-n", "60000", "--rps", "30000", "-d",
                                R"({"value":"{{.RequestNumber}}"})", aAddr);
    BOOST_CHECK_EQUAL(0, sChild.code);
    BOOST_TEST_MESSAGE(sChild.out);
}

void K6Bench()
{
    // port hardcoded in js script
    auto sChild = Util::Perform("unbuffer", "taskset", "-c", "1", "k6", "run", "../k6.js");
    BOOST_CHECK_EQUAL(0, sChild.code);
    BOOST_TEST_MESSAGE(sChild.out);
}

void PyBench()
{
    auto sChild = Util::Perform("unbuffer", "taskset", "-c", "1", "python3", "../bench.py");
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
BOOST_AUTO_TEST_CASE(ghz, *boost::unit_test::disabled())
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
BOOST_AUTO_TEST_CASE(ghz, *boost::unit_test::disabled())
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

    boost::asio::io_context sContext;
    auto                    sFuture = boost::asio::co_spawn(
        sContext,
        [&]() -> boost::asio::awaitable<int> { co_return co_await sClient.Ping(123); },
        boost::asio::use_future);
    sContext.run();
    BOOST_CHECK_EQUAL(123, sFuture.get());
}
BOOST_AUTO_TEST_CASE(generated)
{
    const std::string sAddr = "127.0.0.1:56780";

    struct MyServer : public GRPC::Play::Server
    {
        boost::asio::awaitable<grpc::Status> DoPing(play::PingRequest& aRequest, play::PingResponse& aResponse) override
        {
            aResponse.set_value(aRequest.value());
            co_return grpc::Status::OK;
        };
    };

    MyServer           sServer;
    GRPC::Play::Client sClient(sAddr);

    sServer.Start(sAddr);
    sClient.Start();
    std::this_thread::sleep_for(10ms);

    boost::asio::io_context sContext;
    auto                    sFuture = boost::asio::co_spawn(
        sContext,
        [&]() -> boost::asio::awaitable<int> {
            play::PingRequest sRequest;
            sRequest.set_value(321);
            const auto sResponse = co_await sClient.Ping(sRequest);
            co_return sResponse.value();
        },
        boost::asio::use_future);
    sContext.run();
    BOOST_CHECK_EQUAL(321, sFuture.get());
}
BOOST_AUTO_TEST_CASE(ghz, *boost::unit_test::disabled())
{
    const std::string       sAddr = "127.0.0.1:56780";
    PlayGRPC::TradiasServer sServer;
    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);
    GhzBench(sAddr);
}
BOOST_AUTO_TEST_CASE(k6, *boost::unit_test::disabled())
{
    const std::string       sAddr = "127.0.0.1:56780";
    PlayGRPC::TradiasServer sServer;
    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);
    K6Bench();
}
BOOST_AUTO_TEST_CASE(python, *boost::unit_test::disabled())
{
    const std::string       sAddr = "127.0.0.1:56780";
    PlayGRPC::TradiasServer sServer;
    sServer.Start(sAddr);
    std::this_thread::sleep_for(10ms);
    PyBench();
}
BOOST_AUTO_TEST_SUITE_END()
