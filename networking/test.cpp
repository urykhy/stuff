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
        Udp::Producer sProd("127.0.0.1", PORT);
        for (size_t i = 0; i < COUNT; i++)
            sProd.write(BUFFER.data(), BUFFER.size() + 1);    // asciiZ string
    });

    std::this_thread::sleep_for(50ms);
    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()