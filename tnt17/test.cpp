#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include <threads/Asio.hpp>
#include <threads/Group.hpp>
#include <threads/WaitGroup.hpp>

#include "Cache.hpp"
#include "Client.hpp"

struct DataEntry
{
    int         pk;
    std::string value;

    void parse(MsgPack::imemstream& in)
    {
        using namespace MsgPack;
        uint32_t array_len = read_array_size(in);
        if (array_len != 2)
            throw std::invalid_argument("msgpack array size is not 2");
        read_uint(in, pk);
        read_string(in, value);
    }
    void serialize(MsgPack::omemstream& out) const
    {
        using namespace MsgPack;
        write_array_size(out, 2);
        write_uint(out, pk);
        write_string(out, value);
    }
};

BOOST_AUTO_TEST_SUITE(TNT17)
BOOST_AUTO_TEST_CASE(simple)
{
    Threads::Asio  sLoop;
    Threads::Group sGroup;
    sLoop.start(sGroup, 4); // 4 asio threads

    const auto sAddr = tnt17::endpoint("127.0.0.1", 2090);

    auto sClient = std::make_shared<tnt17::Client>(sLoop.service(), sAddr, 512 /*space id*/);
    sClient->start();
    while (!sClient->is_alive())
        std::this_thread::sleep_for(10ms);

    Threads::WaitGroup sWait(1);
    auto               sRequest = sClient->formatSelect(tnt17::IndexSpec{}.set_id(0), 1);
    sClient->call(sRequest, [&sWait](tnt17::Future&& aResult) {
        tnt17::parse<DataEntry>(aResult, [](auto&& x) {
            BOOST_CHECK_EQUAL(x.pk, 1);
            BOOST_CHECK_EQUAL(x.value, "Roxette");
        });
        sWait.release();
    });
    sWait.wait_for(50ms);

    sClient->stop();
    std::this_thread::sleep_for(10ms);
    sGroup.wait(); // stop threads
}
BOOST_AUTO_TEST_CASE(cache)
{
    Threads::Asio  sLoop;
    Threads::Group sGroup;
    sLoop.start(sGroup);

    const auto sAddr   = tnt17::endpoint("127.0.0.1", 2090);
    auto       sClient = tnt17::cache::Engine(sLoop.service(), sAddr, 513 /*space id*/);
    while (!sClient.is_alive())
        std::this_thread::sleep_for(10ms);

    Threads::WaitGroup sWait(1);
    // delete first, if we have some data from previous run
    sClient.Delete("123", [&sWait](auto&& aResult) {
        auto sResult = aResult.get();
        if (sResult)
            BOOST_TEST_MESSAGE("deleted data: " << sResult->value);
        sWait.release();
    });
    sWait.wait_for(50ms);

    sWait.reset(1);
    sClient.Set(tnt17::cache::Entry{"123", 12, "some data"}, [&sWait](auto&& aResult) {
        auto sResult = aResult.get();
        if (sResult)
            BOOST_TEST_MESSAGE("inserted data: " << sResult->value);
        sWait.release();
    });
    sWait.wait_for(50ms);

    sWait.reset(1);
    sClient.Get("123", [&sWait](tnt17::cache::Engine::Future&& aResult) {
        const auto sResult = aResult.get();
        BOOST_CHECK_EQUAL(sResult->key, "123");
        BOOST_CHECK_EQUAL(sResult->value, "some data");
        sWait.release();
    });
    sWait.wait_for(50ms);

    sWait.reset(1);
    sClient.Delete("123", [&sWait](auto&& aResult) {
        auto sResult = aResult.get();
        if (sResult)
            BOOST_TEST_MESSAGE("deleted data: " << sResult->value);
        sWait.release();
    });
    sWait.wait_for(50ms);

    // fill with data
    sWait.reset(5);
    sClient.Set(tnt17::cache::Entry{"1010", 101, "some data"}, [&sWait](auto&& aResult) { sWait.release(); });
    sClient.Set(tnt17::cache::Entry{"1020", 102, "some data"}, [&sWait](auto&& aResult) { sWait.release(); });
    sClient.Set(tnt17::cache::Entry{"1030", 103, "some data"}, [&sWait](auto&& aResult) { sWait.release(); });
    sClient.Set(tnt17::cache::Entry{"1040", 104, "some data"}, [&sWait](auto&& aResult) { sWait.release(); });
    sClient.Set(tnt17::cache::Entry{"1050", 105, "some data"}, [&sWait](auto&& aResult) { sWait.release(); });
    sWait.wait_for(50ms);

    sWait.reset(1);
    sClient.Expire(104, 10, [&sWait](auto&& aResult) {
        BOOST_CHECK_EQUAL(aResult.get(), 3);
        sWait.release();
    });
    sWait.wait_for(50ms);

    auto sTnt = std::make_shared<tnt17::Client>(sLoop.service(), sAddr, 513 /*space id*/);
    sTnt->start();
    while (!sTnt->is_alive())
        std::this_thread::sleep_for(10ms);

    sWait.reset(1);
    sTnt->call(sTnt->formatSelect(tnt17::IndexSpec{}.set_iterator(tnt17::IndexSpec::ITER_ALL), ""), [&sWait](tnt17::Future&& aResult) {
        unsigned sCount = 0;
        tnt17::parse<tnt17::cache::Entry>(aResult, [&sCount](auto&& x) {
            BOOST_TEST_MESSAGE(x.key << "\t" << x.timestamp << "\t" << x.value);
            sCount++;
        });
        BOOST_CHECK_EQUAL(sCount, 2);
        sWait.release();
    });
    sWait.wait_for(50ms);

    sGroup.wait(); // stop threads
}
BOOST_AUTO_TEST_SUITE_END()
