#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <fmt/core.h>

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include "Client.hpp"

#include <unsorted/Env.hpp>

using namespace std::chrono_literals;

const std::string sRemote = fmt::format("host={} user={} password={}", Util::getEnv("PQ_HOST"), Util::getEnv("PQ_USER"), Util::getEnv("PQ_PASS"));

BOOST_AUTO_TEST_SUITE(pq)
BOOST_AUTO_TEST_CASE(simple)
{
    boost::asio::io_service sAsio;
    PQ::Params              sParams{.single_row_mode = true};
    PQ::Client              sClient(sParams);
    const std::string       sQuery1 = "SELECT id, name FROM card";
    const std::string       sQuery2 = "SELECT NULL";

    auto sFuture = boost::asio::co_spawn(
        sAsio,
        [&]() -> boost::asio::awaitable<void> {
            co_await sClient.Connect(sRemote);
            co_await sClient.Query(sQuery1, [](auto&& sRow) {
                BOOST_TEST_MESSAGE(sRow.Get(0) << " " << sRow.Get(1));
            });
            co_await sClient.Query(sQuery2, [](auto&& sRow) {
                BOOST_TEST_MESSAGE(sRow.IsNull(0));
            });
        },
        boost::asio::use_future);

    sAsio.run_for(1000ms);
    BOOST_REQUIRE_EQUAL(sFuture.wait_for(0ms) == std::future_status::ready, true);
    sFuture.get();
}
BOOST_AUTO_TEST_CASE(timeout)
{
    boost::asio::io_service sAsio;
    PQ::Client              sClient(PQ::Params{});
    const std::string       sQuery = "SELECT PG_SLEEP(0.5), CLOCK_TIMESTAMP();";

    auto sFuture = boost::asio::co_spawn(
        sAsio,
        [&]() -> boost::asio::awaitable<void> {
            using namespace boost::asio::experimental::awaitable_operators;
            boost::asio::steady_timer sTimer(co_await boost::asio::this_coro::executor);
            sTimer.expires_from_now(100ms);
            co_await sClient.Connect(sRemote);
            std::variant<std::monostate, std::monostate> sResult = co_await (sClient.Query(sQuery, [](auto&& sRow) {
                BOOST_TEST_MESSAGE(sRow.Get(1));
            }) || sTimer.async_wait(boost::asio::use_awaitable));
            BOOST_CHECK_EQUAL(sResult.index(), 1); // ensure we got timeout
        },
        boost::asio::use_future);

    sAsio.run_for(1000ms);
    BOOST_REQUIRE_EQUAL(sFuture.wait_for(0ms) == std::future_status::ready, true);
    sFuture.get();
}
BOOST_AUTO_TEST_SUITE_END()
