#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>

#include <boost/asio/use_future.hpp>

#include "Client.hpp"

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(foundationDB)
BOOST_AUTO_TEST_CASE(basic)
{
    FDB::Client sClient;

    // get NX value
    {
        FDB::Transaction sTxn(sClient);
        auto             sFuture = sTxn.Get("foo");
        sFuture.Wait();
        BOOST_CHECK(sFuture.Get() == std::nullopt);
    }

    // set
    {
        FDB::Transaction sTxn(sClient);
        sTxn.Set("foo", "bar");
        BOOST_CHECK_NO_THROW(sTxn.Commit());
    }

    // get existing value
    {
        FDB::Transaction sTxn(sClient);
        auto             sFuture = sTxn.Get("foo");
        sFuture.Wait();
        BOOST_CHECK(*sFuture.Get() == "bar");
    }

    // clear
    {
        FDB::Transaction sTxn(sClient);
        sTxn.Erase("foo");
        BOOST_CHECK_NO_THROW(sTxn.Commit());
    }
}
BOOST_AUTO_TEST_CASE(async)
{
    boost::asio::io_service sAsio;
    FDB::Client             sClient;

    auto sFuture = boost::asio::co_spawn(
        sAsio,
        [&]() mutable -> boost::asio::awaitable<void> {
            // get NX value
            {
                FDB::Transaction sTxn(sClient);
                auto             sFuture = sTxn.Get("foo");
                co_await sFuture.CoWait();
                BOOST_CHECK(sFuture.Get() == std::nullopt);
            }

            // set
            {
                FDB::Transaction sTxn(sClient);
                sTxn.Set("foo", "bar");
                BOOST_CHECK_NO_THROW(co_await sTxn.CoCommit());
            }

            // get existing value
            {
                FDB::Transaction sTxn(sClient);
                auto             sFuture = sTxn.Get("foo");
                co_await sFuture.CoWait();
                BOOST_CHECK(*sFuture.Get() == "bar");
            }

            // clear
            {
                FDB::Transaction sTxn(sClient);
                sTxn.Erase("foo");
                BOOST_CHECK_NO_THROW(co_await sTxn.CoCommit());
            }
        },
        boost::asio::use_future);
    sAsio.run_for(500ms);
    sFuture.get();
}
BOOST_AUTO_TEST_SUITE_END()
