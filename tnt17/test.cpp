#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <threads/Asio.hpp>
#include <threads/Group.hpp>
#include <threads/WaitGroup.hpp>
#include <serialize/MsgPack.hpp>
#include "Client.hpp"

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

#if 0
    auto sClient = std::make_shared<C>(sLoop.service(), sAddr, 512 /*space id*/, [&sWait](std::exception_ptr aPtr){
        sWait.release();
        if (nullptr == aPtr) {
            BOOST_TEST_MESSAGE("connected");
        } else {
            try {
                std::rethrow_exception(aPtr);
            } catch (const Event::NetworkError& e) {
                BOOST_TEST_MESSAGE("no connection: " << e.what());
            }
        }
    });
    sClient->select(sIndex, 1 /*key*/ ,[](std::future<std::vector<DataEntry>>&& aResult){
        try {
            const auto sResult = aResult.get();
        } catch (const Event::NetworkError& e) {
            BOOST_CHECK_EQUAL(e.what(), "network error: Transport endpoint is not connected");
        }
    });

    sClient->start();
    sWait.wait();
    BOOST_REQUIRE(sClient->is_connected());

    // make calls, callback will be called in asio thread
    sWait.reset(2);
    sClient->select(sIndex, 1 /*key*/ ,[&sWait](std::future<std::vector<DataEntry>>&& aResult){
        const auto sResult = aResult.get();
        BOOST_REQUIRE_EQUAL(sResult.size(), 1);
        BOOST_CHECK_EQUAL(sResult[0].pk, 1);
        BOOST_CHECK_EQUAL(sResult[0].value, "Roxette");
        sWait.release();
    });

    sClient->select(tnt17::IndexSpec().set_id(1), "NightWish" /*key*/ ,[&sWait](std::future<std::vector<DataEntry>>&& aResult){
        const auto sResult = aResult.get();
        BOOST_REQUIRE_EQUAL(sResult.size(), 2);
        BOOST_CHECK_EQUAL(sResult[0].value, "NightWish");
        BOOST_CHECK_EQUAL(sResult[1].value, "NightWish");
        sWait.release();
    });

    sWait.wait();
    sClient->stop();
    std::this_thread::sleep_for(200ms);
#endif

}
BOOST_AUTO_TEST_SUITE_END()
