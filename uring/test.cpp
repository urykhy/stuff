#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <chrono>
using namespace std::chrono_literals;

#include <array>

#include <networking/Resolve.hpp>
#include <networking/TcpSocket.hpp>
#include <parser/Hex.hpp>
#include <threads/WaitGroup.hpp>

#include "Server.hpp"
#include "URing.hpp"

BOOST_AUTO_TEST_SUITE(uring)
BOOST_AUTO_TEST_CASE(simple)
{
    Util::URing    sRing;
    Threads::Group sGroup;
    sRing.start(sGroup);

    Tcp::Socket sServer;
    sServer.set_reuse_port();
    sServer.bind(2090);
    sServer.listen();
    BOOST_TEST_MESSAGE("listen on " << sServer.get_fd());

    struct CB : public Util::URingUser
    {
        using F = std::function<void(int, int)>;
        F m_Parent;
        CB(F f)
        : m_Parent(f)
        {}
        void on_event(int aKind, int32_t aRes) override { m_Parent(aKind, aRes); }
    };

    int sServerPeer = -1;
    //Util::Raii sCleanup1([&sServerPeer](){ if (sServerPeer > -1) close(sServerPeer); });
    sRing.accept(sServer.get_fd(), std::make_shared<CB>([&sServerPeer](int aKind, int32_t aRes) {
                     BOOST_TEST_MESSAGE("accept callback: " << aRes);
                     if (aRes > 0)
                         sServerPeer = aRes;
                 }));

    Tcp::Socket sClient = Tcp::Socket();
    BOOST_TEST_MESSAGE("client socket " << sClient.get_fd());
    struct sockaddr_in sPeer;
    memset(&sPeer, 0, sizeof(sPeer));
    sPeer.sin_family      = AF_INET;
    sPeer.sin_port        = htons(2090);
    sPeer.sin_addr.s_addr = Util::resolveName("127.0.0.1");
    BOOST_TEST_MESSAGE("connecting to " << sPeer.sin_addr.s_addr);
    socklen_t sPeerLen = sizeof(sPeer);

    Threads::WaitGroup sWait(1);
    sRing.connect(sClient.get_fd(), std::make_shared<CB>([&sWait, &sClient](int aKind, int32_t aRes) {
                      std::string sComment;
                      if (aRes < 0)
                          sComment = ", error: " + std::to_string(aRes);
                      BOOST_TEST_MESSAGE("connect callback: " << aRes << sComment);
                      sWait.release();
                  }),
                  (sockaddr*)&sPeer, &sPeerLen);
    sWait.wait(); // wait for connection

    if (sServerPeer < 0) {
        BOOST_TEST_MESSAGE("fail to connect/accept");
        return;
    }

    std::string sData{"0123456789"};
    sWait.reset(1);
    sRing.write(sServerPeer, std::make_shared<CB>([&sWait](int aKind, int32_t aRes) {
                    BOOST_TEST_MESSAGE("written: " << aRes);
                    sWait.release();
                }),
                iovec{sData.data(), sData.size()});
    sWait.wait();

    std::string sExpected;
    sExpected.resize(128);
    sWait.reset(1);
    sRing.read(sClient.get_fd(), std::make_shared<CB>([&sWait, &sExpected](int aKind, int32_t aRes) mutable {
                   BOOST_TEST_MESSAGE("readed: " << aRes);
                   sExpected.resize(aRes);
                   sWait.release();
               }),
               iovec{sExpected.data(), sExpected.size()});
    sWait.wait();

    BOOST_CHECK_EQUAL(sData, sExpected);

    sWait.reset(1);
    sRing.close(sServerPeer, std::make_shared<CB>([&sWait](int aKind, int32_t aRes) {
                    BOOST_TEST_MESSAGE("close callback: " << aRes);
                    sWait.release();
                }));
    sWait.wait();
}
BOOST_AUTO_TEST_CASE(Httpd)
{
    Util::URing    sRing;
    Threads::Group sGroup;
    sRing.start(sGroup);

    Tcp::Socket sServer;
    sServer.set_reuse_port();
    sServer.bind(2090);
    sServer.listen();
    BOOST_TEST_MESSAGE("listen on " << sServer.get_fd());

    sRing.accept(sServer.get_fd(), std::make_shared<URing::http::Listener>(sRing, sServer.get_fd()));

    std::this_thread::sleep_for(1s);
}
BOOST_AUTO_TEST_SUITE_END()
