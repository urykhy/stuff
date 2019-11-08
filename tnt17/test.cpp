#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>

#include <threads/Asio.hpp>
#include <threads/Group.hpp>
#include <serialize/MsgPack.hpp>
#include "Client.hpp"

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
    Threads::Group sGroup;
    Threads::Asio  sLoop;
    sLoop.start(1, sGroup);

    const auto sAddr = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 2090);
    tnt17::Client<DataEntry> sClient(sLoop.service(), sAddr, 512 /*space id*/);

    std::this_thread::sleep_for(200ms);

    // make calls, callback will be called in asio thread
    sClient.select(0 /*index*/, 1 /*key*/ ,[](std::future<std::vector<DataEntry>>&& aResult){
        const auto sResult = aResult.get();
        BOOST_CHECK_EQUAL(sResult.size(), 1);
        BOOST_CHECK_EQUAL(sResult[0].pk, 1);
        BOOST_CHECK_EQUAL(sResult[0].value, "Roxette");
    });

    std::this_thread::sleep_for(200ms);
    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()
