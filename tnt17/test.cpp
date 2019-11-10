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
    sLoop.start(1, sGroup);

    tnt17::IndexSpec sIndex;

    Threads::WaitGroup sWait(1);
    const auto sAddr = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 2090);
    using C = tnt17::Client<DataEntry>;
    auto sClient = std::make_shared<C>(sLoop.service(), sAddr, 512 /*space id*/, [&sWait](std::exception_ptr aPtr){
        sWait.release();
        if (nullptr == aPtr) {
            BOOST_TEST_MESSAGE("connected");
        } else {
            try {
                std::rethrow_exception(aPtr);
            } catch (const std::exception& e) {
                BOOST_TEST_MESSAGE("no connection: " << e.what());
            }
        }
    });
    sClient->select(sIndex, 1 /*key*/ ,[](std::future<std::vector<DataEntry>>&& aResult){
        try {
            const auto sResult = aResult.get();
        } catch (const std::exception& e) {
            BOOST_CHECK_EQUAL(e.what(), "network error: Transport endpoint is not connected");
        }
    });

    sClient->start();
    sWait.wait();
    BOOST_REQUIRE(sClient->is_open());

    if (!sClient->is_open()) {
        BOOST_TEST_MESSAGE("no connection: exiting");
        sGroup.wait();
        return;
    }

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
    //std::this_thread::sleep_for(200ms);

    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()
