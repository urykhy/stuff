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

    // read range
    {
        // fill
        {
            FDB::Transaction sTxn1(sClient);
            sTxn1.EraseRange("goo", "goo9");
            sTxn1.Set("goo1", "bar1");
            sTxn1.Set("goo4", "bar4");
            sTxn1.Set("goo6", "bar6");
            sTxn1.Set("goo7", "bar7");
            BOOST_CHECK_NO_THROW(sTxn1.Commit());
        }

        // read range
        FDB::Transaction sTxn2(sClient);
        auto             sFuture = sTxn2.GetRange("goo", "goo7", 2);
        sFuture.Wait();
        auto sArray = sFuture.GetRange();
        BOOST_CHECK_EQUAL(sArray.size(), 2);
        for (auto& x : sArray) {
            BOOST_TEST_MESSAGE("got key " << x.first << " with value: " << x.second);
        }

        // while txt2 exists, insert to middle of the range
        {
            FDB::Transaction sTxn1(sClient);
            sTxn1.Set("goo5", "bar5");
            BOOST_CHECK_NO_THROW(sTxn1.Commit());
        }

        // no conflict. data silently removed.
        sTxn2.EraseRange("goo", "goo7");
        BOOST_CHECK_NO_THROW(sTxn2.Commit());

        {
            FDB::Transaction sTxn(sClient);
            auto             sFuture = sTxn.Get("goo5");
            sFuture.Wait();
            BOOST_CHECK(sFuture.Get() == std::nullopt);
        }
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
