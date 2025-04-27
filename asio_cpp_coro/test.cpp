#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>

#include "lib/API.hpp"
#include "lib/Asio.hpp"
#include "lib/Hedging.hpp"

#include <time/Meter.hpp>

using namespace std::chrono_literals;
using namespace AsioHttp;

BOOST_AUTO_TEST_SUITE(asio_http)
BOOST_AUTO_TEST_CASE(simple)
{
    boost::asio::io_service sAsio;

    auto sServer = createServer({});
    auto sClient = createClient({}); // default port 3080

    auto sServerFuture = boost::asio::co_spawn(
        sAsio,
        [&]() -> ba::awaitable<void> {
            sServer->addHandler("/test", [](BeastRequest&& aRequest) -> boost::asio::awaitable<BeastResponse> {
                BeastResponse sResponse;
                sResponse.result(bb::http::status::ok);
                sResponse.body() = "body_1";
                co_return sResponse;
            });
            co_return co_await sServer->run();
        },
        boost::asio::use_future);

    auto sFuture = boost::asio::co_spawn(
        sAsio,
        [&]() -> ba::awaitable<void> {
            auto sRequest = Request{.method = "GET", .url = "http://localhost:3080/test"};
            auto sResult  = co_await sClient->perform(std::move(sRequest));
            BOOST_CHECK_EQUAL(sResult.status, 200);
            BOOST_CHECK_EQUAL(sResult.body, "body_1");
            co_return;
        },
        boost::asio::use_future);

    sAsio.run_for(50ms);
    BOOST_REQUIRE_EQUAL(sFuture.wait_for(0ms) == std::future_status::ready, true);
    sFuture.get();

    // get server error (if any)
    if (sServerFuture.wait_for(0ms) == std::future_status::ready) {
        sServerFuture.get();
    }
}

BOOST_AUTO_TEST_CASE(keep_alive)
{
    boost::asio::io_service sAsio;

    auto sServer = createServer({});
    auto sClient = createClient({.alive = true}); // default port 3080

    auto sServerFuture = boost::asio::co_spawn(
        sAsio,
        [&]() -> ba::awaitable<void> {
            sServer->addHandler("/test", [](BeastRequest&& aRequest) -> boost::asio::awaitable<BeastResponse> {
                BeastResponse sResponse;
                sResponse.result(bb::http::status::ok);
                sResponse.body() = "body_1";
                co_return sResponse;
            });
            co_return co_await sServer->run();
        },
        boost::asio::use_future);

    auto sFuture = boost::asio::co_spawn(
        sAsio,
        [&]() -> ba::awaitable<void> {
            auto sRequest = Request{.method = "GET", .url = "http://localhost:3080/test"};
            auto sResult  = co_await sClient->perform(std::move(sRequest));
            BOOST_CHECK_EQUAL(sResult.status, 200);
            BOOST_CHECK_EQUAL(sResult.body, "body_1");
            sResult = co_await sClient->perform(std::move(sRequest));
            BOOST_CHECK_EQUAL(sResult.status, 200);
            BOOST_CHECK_EQUAL(sResult.body, "body_1");
            co_return;
        },
        boost::asio::use_future);

    sAsio.run_for(50ms);
    BOOST_REQUIRE_EQUAL(sFuture.wait_for(0ms) == std::future_status::ready, true);
    sFuture.get();

    // get server error (if any)
    if (sServerFuture.wait_for(0ms) == std::future_status::ready) {
        sServerFuture.get();
    }
}

BOOST_AUTO_TEST_CASE(hedge)
{
    boost::asio::io_service sAsio;

    auto sServer  = createServer({});
    auto sClient  = createClient({.alive = true}); // default port 3080
    auto sHedging = AsioHttp::Hedging(sClient);

    int  sRequestCounter = 0;
    auto sServerFuture   = boost::asio::co_spawn(
        sAsio,
        [&]() -> ba::awaitable<void> {
            sServer->addHandler("/test", [&](BeastRequest&& aRequest) -> boost::asio::awaitable<BeastResponse> {
                BeastResponse sResponse;
                sResponse.result(bb::http::status::ok);
                sResponse.body() = "body_1";
                sRequestCounter++;
                if (sRequestCounter == 13 or sRequestCounter == 7) {
                    BOOST_TEST_MESSAGE("slow response");
                    ba::steady_timer sTimer(co_await boost::asio::this_coro::executor);
                    sTimer.expires_from_now(20ms);
                    co_await sTimer.async_wait(ba::use_awaitable);
                }
                co_return sResponse;
            });
            co_return co_await sServer->run();
        },
        boost::asio::use_future);

    const int COUNT   = 15;
    auto      sFuture = boost::asio::co_spawn(
        sAsio,
        [&]() -> ba::awaitable<void> {
            auto sRequest = Request{.method = "GET", .url = "/test"};
            for (int i = 0; i < COUNT; i++) {
                Time::Meter sMeter;
                auto        sResult = co_await sHedging.perform({
                                .request    = sRequest,
                                .timeout_ms = 5,
                                .peer       = []() { return "localhost:3080"; },
                                .hedge      = [](const std::string&) { return "localhost:3080"; },
                });
                if (sRequestCounter == 7) {
                    BOOST_CHECK_GE(sMeter.get().to_ms(), 20); // slow response, hedging have no budget
                } else {
                    BOOST_CHECK_LE(sMeter.get().to_ms(), 10); // fast responses
                }
            }
            co_return;
        },
        boost::asio::use_future);

    sAsio.run_for(50ms);
    BOOST_REQUIRE_EQUAL(sFuture.wait_for(0ms) == std::future_status::ready, true);
    sFuture.get();

    // get server error (if any)
    if (sServerFuture.wait_for(0ms) == std::future_status::ready) {
        sServerFuture.get();
    }
}
BOOST_AUTO_TEST_SUITE_END()
