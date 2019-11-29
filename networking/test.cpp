#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>

#include "UdpPipe.hpp"

using namespace std::chrono_literals;

struct Message {    // data over UDP
    char data[8];
} __attribute__ ((packed));

BOOST_AUTO_TEST_SUITE(Networking)
BOOST_AUTO_TEST_CASE(resolve)
{
    BOOST_CHECK_EQUAL(0x100007F, Util::resolveAddr("127.0.0.1"));
    BOOST_CHECK_EQUAL(0x100007F, Util::resolveName("127.0.0.1"));
    BOOST_CHECK_EQUAL(0x100007F, Util::resolveName("localhost"));
    BOOST_CHECK_THROW([](){ Util::resolveName("nx.domain.qjz9zk"); }(), std::runtime_error);
}
BOOST_AUTO_TEST_CASE(socket)
{
    Udp::Socket s;
    auto sBufSize = s.getBufSize();
    BOOST_TEST_MESSAGE("buffer size: " << sBufSize.first << "/" << sBufSize.second);

    s.setBufSize(4096, 16384);
    sBufSize = s.getBufSize();
    // kernel internally doubles that size
    BOOST_TEST_MESSAGE("buffer size: " << sBufSize.first << "/" << sBufSize.second);
    BOOST_CHECK(sBufSize.first  == 8192);
    BOOST_CHECK(sBufSize.second == 32768);

}
BOOST_AUTO_TEST_CASE(pipe)
{
    const std::string  BUFFER = "test123";
    constexpr size_t   COUNT  = 12;
    constexpr uint16_t PORT   = 2091;

    Threads::Group sGroup;
    sGroup.start([&](){
        Threads::Group sCG;
        Udp::Consumer<Message> sConsumer(6, PORT, [&](auto& aMsg){
            BOOST_CHECK_EQUAL(BUFFER.size() + 1, aMsg.size);
            BOOST_CHECK_EQUAL(BUFFER, aMsg.data.data);
        });
        sConsumer.start(sCG);
        std::this_thread::sleep_for(100ms);
        sCG.wait();
    });
    std::this_thread::sleep_for(50ms);

    sGroup.start([&](){
        Udp::Producer sProd("localhost", PORT);
        for (size_t i = 0; i < COUNT; i++)
            sProd.write(BUFFER.data(), BUFFER.size() + 1);    // asciiZ string
    });

    std::this_thread::sleep_for(50ms);
    sGroup.wait();
}
BOOST_AUTO_TEST_CASE(mread)
{
    constexpr unsigned MSG_COUNT = 4;

    Udp::Socket consumer(2092); // listen on port
    consumer.set_nonblocking(); // avoid hang at mread
    Udp::Socket producer(Util::resolveName("127.0.0.1"), 2092); // `connect` to port

    producer.write("1234567", 8);
    producer.write("abcdefg", 8);
    producer.write("ABCDEFG", 8);
    producer.write("HIJKLMN", 8);
    std::this_thread::sleep_for(100ms);

    std::array<Message, MSG_COUNT> sData;
    Udp::MultiBuffer<MSG_COUNT> sHeader;
    for (unsigned i = 0; i < MSG_COUNT; i++)
        sHeader.append(&sData[i], sizeof(Message));

    int rc = consumer.mread(sHeader.buffer(), sHeader.size());
    BOOST_CHECK_EQUAL(0, consumer.getError());
    BOOST_TEST_MESSAGE("got " << rc << " messages");
    BOOST_REQUIRE_EQUAL(rc, MSG_COUNT);

    for (unsigned i = 0; i < MSG_COUNT; i++)
        BOOST_CHECK_EQUAL(sizeof(Message), sHeader.size(i));

    BOOST_CHECK_EQUAL("1234567", sData[0].data);
    BOOST_CHECK_EQUAL("abcdefg", sData[1].data);
    BOOST_CHECK_EQUAL("ABCDEFG", sData[2].data);
    BOOST_CHECK_EQUAL("HIJKLMN", sData[3].data);
}
BOOST_AUTO_TEST_SUITE_END()