#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>

#include <boost/asio/use_future.hpp>

#include "Client.hpp"
#include "Overlap.hpp"

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

    // 2 transactions, update same value
    {
        FDB::Transaction sTxn1(sClient);
        auto             sFuture1 = sTxn1.Get("foo1");
        sFuture1.Wait();
        sTxn1.Set("foo1", "bar");

        FDB::Transaction sTxn2(sClient);
        auto             sFuture2 = sTxn2.Get("foo1");
        sFuture2.Wait();
        sTxn2.Set("foo1", "bar");

        BOOST_CHECK_NO_THROW(sTxn1.Commit());
        BOOST_CHECK_EXCEPTION(sTxn2.Commit(), std::runtime_error, [](auto e) {
            return e.what() == std::string("future get error: Transaction not committed due to conflict with another transaction (1020)");
        });
    }
}

BOOST_AUTO_TEST_CASE(overlap)
{
    boost::asio::io_service sAsio;
    FDB::Client             sClient;

    auto sFuture = boost::asio::co_spawn(
        sAsio,
        [&]() mutable -> boost::asio::awaitable<void> {
            FDB::Overlap sOverlap(sClient);
            co_await sOverlap.Start();
            for (int i = 0; i < 10; i++) {
                FDB::Transaction sTxn(sClient);
                if (auto sVersion = sOverlap.GetVersionTimestamp(); sVersion > 0) {
                    BOOST_TEST_MESSAGE("use version: " << sVersion);
                    sTxn.SetVersionTimestamp(sVersion);
                }

                auto sFuture = sTxn.Get("foo1");
                sFuture.Wait();
                BOOST_TEST_MESSAGE(*sFuture.Get());

                boost::asio::steady_timer sTimer(co_await boost::asio::this_coro::executor);
                sTimer.expires_from_now(std::chrono::seconds(1));
                co_await sTimer.async_wait(boost::asio::use_awaitable);
            }
            sOverlap.Stop();
        },
        boost::asio::use_future);
    sAsio.run_for(15s);
    sFuture.get();
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
