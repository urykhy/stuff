#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <threads/Asio.hpp>
#include <threads/Group.hpp>
#include <threads/WaitGroup.hpp>

#include "Client.hpp"
#include "Fetch.hpp"

#include <chrono>
using namespace std::chrono_literals;

struct DataEntry {
    int pk;
    std::string value;

    void parse(MsgPack::imemstream& in)
    {
        using namespace MsgPack;
        uint32_t array_len = read_array_size(in);
        assert (array_len >= 2);
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

    template<class S>
    static void formatKey(S& aStream, const int aKey) {
        MsgPack::write_uint(aStream, aKey);
    }

    template<class S>
    static void formatKey(S& aStream, const std::string& aKey) {
        MsgPack::write_string(aStream, aKey);
    }
};

BOOST_AUTO_TEST_SUITE(TNT17)
BOOST_AUTO_TEST_CASE(simple)
{
    Threads::Asio  sLoop;
    Threads::Group sGroup;
    sLoop.start(4, sGroup); // 4 asio threads

    const auto sAddr = tnt17::endpoint("127.0.0.1", 2090);
    using C = tnt17::Client<DataEntry>;

    auto sClient = std::make_shared<C>(sLoop.service(), sAddr, 512 /*space id*/);
    sClient->start();
    while (!sClient->is_alive()) std::this_thread::sleep_for(10ms);

    Threads::WaitGroup sWait(1);
    auto sRequest = sClient->formatSelect(tnt17::IndexSpec{}.set_id(0), 1);
    bool sQueued  = sClient->call(sRequest.first, sRequest.second, [&sWait](typename C::Future&& aResult)
    {
        const auto sResult = aResult.get();
        BOOST_REQUIRE_EQUAL(sResult.size(), 1);
        BOOST_CHECK_EQUAL(sResult[0].pk, 1);
        BOOST_CHECK_EQUAL(sResult[0].value, "Roxette");
        sWait.release();
    });
    BOOST_CHECK_EQUAL(sQueued, true);
    sWait.wait_for(50ms);

    sClient->stop();
    std::this_thread::sleep_for(10ms);
    sGroup.wait();  // stop threads
}
BOOST_AUTO_TEST_CASE(fetch)
{
    Threads::Asio  sLoop;
    Threads::Group sGroup;
    sLoop.start(4, sGroup); // 4 asio threads

    const auto sAddr = tnt17::endpoint("127.0.0.1", 2090);
    using C = tnt17::Client<DataEntry>;

    auto sClient = std::make_shared<C>(sLoop.service(), sAddr, 512 /*space id*/);
    sClient->start();

    using FQ = tnt17::FetchQueue<DataEntry>;
    using FE = tnt17::Fetch<DataEntry>;
    FQ sQueue;
    sQueue.requests = {1,2,3,4,10,11,12};

    auto sFetch = std::make_shared<FE>(sClient, sQueue);
    sFetch->start();
    while (!sFetch->is_done()) std::this_thread::sleep_for(1ms);

    BOOST_TEST_MESSAGE("fetch completed");

    unsigned sKeys = 0;
    for (auto& x : sQueue.responses)
    {
        switch (x.pk)
        {
        case 1: BOOST_CHECK_EQUAL(x.value, "Roxette"); break;
        case 2: BOOST_CHECK_EQUAL(x.value, "Scorpions"); break;
        case 3: BOOST_CHECK_EQUAL(x.value, "Ace of Base"); break;
        case 4: BOOST_CHECK(false); break;  // there is no key in tnt, so - no data
        case 10: BOOST_CHECK_EQUAL(x.value, "NightWish"); break;
        case 11: BOOST_CHECK_EQUAL(x.value, "NightWish"); break;
        case 12: BOOST_CHECK_EQUAL(x.value, "NightWish"); break;
        }
        sKeys++;
    }
    BOOST_CHECK_EQUAL(sKeys, 6); // 1,2,3,10,11,12 must be found

    sFetch->stop();
    sClient->stop();
    std::this_thread::sleep_for(10ms);
    sGroup.wait();  // stop threads
}
BOOST_AUTO_TEST_SUITE_END()
