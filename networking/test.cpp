#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>

#include "EPoll.hpp"
#include "EventFd.hpp"
#include "MultiBuffer.hpp"
#include "SRV.hpp"
#include "TcpSocket.hpp"
#include "TimerFd.hpp"
#include "UdpSocket.hpp"

#include <unsorted/Taskset.hpp>

using namespace std::chrono_literals;

struct Message
{ // data over UDP
    char data[8];
} __attribute__((packed));

BOOST_AUTO_TEST_SUITE(Networking)
BOOST_AUTO_TEST_CASE(resolve)
{
    BOOST_CHECK_EQUAL(0x100007F, Util::resolveAddr("127.0.0.1"));
    BOOST_CHECK_EQUAL(0x100007F, Util::resolveName("127.0.0.1"));
    BOOST_CHECK_EQUAL(0x100007F, Util::resolveName("localhost"));
    BOOST_CHECK_THROW([]() { Util::resolveName("nx.domain.qjz9zk"); }(), std::runtime_error);

    Util::SRV sResolver;
    auto      sSet = sResolver("_kerberos._tcp.kerberos.elf.dark");
    BOOST_CHECK(sSet.size() > 0);
    for (auto& x : sSet)
        BOOST_TEST_MESSAGE("resolved to " << x.name << ":" << x.port << " with priority: " << x.prio << ", weight: " << x.weight);
    bool rc = sSet[0].resolve() == Util::SRV::HostPort{0x030A670A, 88};
    BOOST_CHECK(rc);
}
BOOST_AUTO_TEST_CASE(socket)
{
    Udp::Socket s;
    auto        sBufSize = s.get_buffer();
    BOOST_TEST_MESSAGE("buffer size: " << sBufSize.first << "/" << sBufSize.second);

    s.set_buffer(4096, 16384);
    sBufSize = s.get_buffer();
    // kernel internally doubles that size
    BOOST_TEST_MESSAGE("buffer size: " << sBufSize.first << "/" << sBufSize.second);
    BOOST_CHECK(sBufSize.first == 8192);
    BOOST_CHECK(sBufSize.second == 32768);
}
BOOST_AUTO_TEST_CASE(mread)
{
    constexpr unsigned MSG_COUNT = 4;

    Udp::Socket consumer(2092);                                 // listen on port
    consumer.set_nonblocking();                                 // avoid hang at mread
    Udp::Socket producer(Util::resolveName("127.0.0.1"), 2092); // `connect` to port

    producer.write("1234567", 8);
    producer.write("abcdefg", 8);
    producer.write("ABCDEFG", 8);
    producer.write("HIJKLMN", 8);
    BOOST_TEST_MESSAGE("TIOCOUTQ: " << producer.outq());
    std::this_thread::sleep_for(100ms);

    std::array<Message, MSG_COUNT> sData;
    Util::MultiBuffer<MSG_COUNT>   sHeader;
    for (unsigned i = 0; i < MSG_COUNT; i++)
        sHeader.append(&sData[i], sizeof(Message));

    int rc = consumer.read(sHeader.buffer(), sHeader.size());
    BOOST_CHECK_EQUAL(0, consumer.get_error());
    BOOST_TEST_MESSAGE("got " << rc << " messages");
    BOOST_REQUIRE_EQUAL(rc, MSG_COUNT);

    for (unsigned i = 0; i < MSG_COUNT; i++)
        BOOST_CHECK_EQUAL(sizeof(Message), sHeader.size(i));

    BOOST_CHECK_EQUAL("1234567", sData[0].data);
    BOOST_CHECK_EQUAL("abcdefg", sData[1].data);
    BOOST_CHECK_EQUAL("ABCDEFG", sData[2].data);
    BOOST_CHECK_EQUAL("HIJKLMN", sData[3].data);
}
BOOST_AUTO_TEST_CASE(eventfd)
{
    Util::EventFd sFd;
    sFd.signal();
    sFd.signal();
    BOOST_CHECK_EQUAL(2, sFd.read());
}
BOOST_AUTO_TEST_CASE(timerfd)
{
    Util::setCore(1);
    Util::TimerFd sFd;
    std::this_thread::sleep_for(50ms);
    int sCount = sFd.read();
    BOOST_TEST_MESSAGE("10 ms timer wake up " << sCount << " times in 50ms");
    BOOST_CHECK(sCount >= 5);

    sFd.cancel();
    std::this_thread::sleep_for(50ms);
    sCount = sFd.read();
    BOOST_CHECK(sCount == 0);
}
BOOST_AUTO_TEST_CASE(tcp)
{
    Threads::Group sGroup;
    sGroup.start([]() {
        Tcp::Socket sServer;
        sServer.set_reuse_port();
        sServer.bind(2082);
        sServer.listen();
        int sFd = sServer.accept();
        BOOST_CHECK_GE(sFd, 0);
        Tcp::Socket sPeer = Tcp::Socket(sFd);
        BOOST_TEST_MESSAGE("connection from " << Util::formatAddr(sPeer.get_peer()));
        std::string sData(10, ' ');
        sPeer.read(sData.data(), 10);
        sPeer.write(sData.data(), 10);
    });
    std::this_thread::sleep_for(10ms);

    Tcp::Socket sClient = Tcp::Socket();
    sClient.connect(Util::resolveName("127.0.0.1"), 2082);
    sClient.write("1234567890", 10);
    std::string sReply(10, ' ');
    sClient.read(sReply.data(), 10);
    BOOST_CHECK_EQUAL("1234567890", sReply);
}
BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE(epoll)
BOOST_AUTO_TEST_CASE(ping)
{
    Util::EPoll    sEpoll;
    Threads::Group sGroup;
    sEpoll.start(sGroup);

    struct PingHandler : public Util::EPoll::HandlerFace
    {
        Udp::Socket m_Socket;
        PingHandler()
        : m_Socket(2092)
        {}
        int get_fd() { return m_Socket.get_fd(); }

        Result on_read() override
        {
            auto sMsg = m_Socket.read();
            BOOST_CHECK_EQUAL(sMsg.size, 4);
            BOOST_TEST_MESSAGE("message: " << sMsg.message << " from " << Util::formatAddr(sMsg.addr));
            Udp::Socket::Msg sReply{0, "pong", sMsg.addr};
            m_Socket.write(sReply);
            return m_Socket.ionread() ? Result::RETRY : Result::OK;
        }
        Result on_write() override { return Result::OK; }
        void   on_error() override { BOOST_CHECK(false); }
    };
    auto sHandler = std::make_shared<PingHandler>();
    sEpoll.post([sHandler](Util::EPoll* ptr) {
        ptr->insert(sHandler->get_fd(), EPOLLIN, sHandler);
    });
    std::this_thread::sleep_for(10ms);

    // create other socket, send message, wait for response
    Udp::Socket sProducer(Util::resolveName("127.0.0.1"), 2092); // `connect` to port
    BOOST_TEST_MESSAGE("producer socket bound to " << sProducer.get_port());
    sProducer.write("ping", 4);
    std::this_thread::sleep_for(20ms);

    auto sMsg = sProducer.read();
    BOOST_TEST_MESSAGE("message: " << sMsg.message << " from " << Util::formatAddr(sMsg.addr));
    BOOST_CHECK_EQUAL(sMsg.message, "pong");

    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()
