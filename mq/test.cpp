#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <MessageQueue.hpp>
#include <UdpQueue.hpp>
#include <WorkQ.hpp>
#include <Periodic.hpp> // for sleep

struct DummySender : MQ::SenderTransport
{
    MQ::aux::Sender* m_Back = nullptr;
    size_t m_Action = 0;
    void push(size_t aSerial, const std::string& aData, size_t aMinSerial) override
    {
        m_Action++;
        switch (m_Action)
        {
        case 1:
            BOOST_CHECK_EQUAL(aData, "test123");
            m_Back->ack(aSerial);
            break;
        case 2:
            m_Back->restore(aSerial);
            break;
        case 3:
            BOOST_CHECK_EQUAL(aData, "test456test789");
            m_Back->ack(aSerial);
            break;
        }
    }
};

BOOST_AUTO_TEST_SUITE(MQ)
BOOST_AUTO_TEST_CASE(sender)
{
    Threads::Group sGroup;
    Threads::WorkQ sAsio;
    sAsio.start(1, sGroup);

    DummySender sDummy;
    MQ::aux::Sender sSender(sAsio, &sDummy);
    sDummy.m_Back = &sSender;

    sSender.push("test123");
    Threads::sleep(1.1);
    sSender.push("test456");
    sSender.push("test789");
    Threads::sleep(1.1);

    sAsio.term();
    sGroup.wait();
}
BOOST_AUTO_TEST_CASE(receiver)
{
    auto sHandler = [sAction = 0](std::string&& sData) mutable {
        sAction++;
        switch (sAction)
        {
            case 1: BOOST_CHECK_EQUAL(sData, "test123"); break;
            case 2: BOOST_CHECK_EQUAL(sData, "test456"); break;
        }
    };

    MQ::aux::Receiver sRecv(sHandler);
    sRecv.push(1, "test123");
    sRecv.push(1, "test123");
    sRecv.push(2, "test456");
    BOOST_CHECK_EQUAL(sRecv.size(), 2);
}
BOOST_AUTO_TEST_CASE(udp)
{
    Threads::Group sGroup;
    Threads::WorkQ sAsio;
    sAsio.start(1, sGroup);

    MQ::UDP::Config sRecvConfig{"0.0.0.0", 4567};   // LISTEN
    MQ::UDP::Config sSendConfig{"127.0.0.1", 4567}; // CONNECT TO

    MQ::UDP::Receiver sReceiver(sRecvConfig, sAsio, [](std::string&& aData) {
        BOOST_CHECK_EQUAL(aData, "test123456");
    });
    MQ::UDP::Sender sSender(sSendConfig, sAsio);

    sSender.push("test123");
    sSender.push("456");
    Threads::sleep(1.1);
    BOOST_CHECK_EQUAL(sSender.size(), 0);

    sAsio.term();
    sGroup.wait();
}
BOOST_AUTO_TEST_CASE(retry)
{
    Threads::Group sGroup;
    Threads::WorkQ sAsio;
    sAsio.start(1, sGroup);

    MQ::UDP::Config sRecvConfig{"0.0.0.0", 4567};   // LISTEN
    MQ::UDP::Config sSendConfig{"127.0.0.1", 4567}; // CONNECT TO

    MQ::UDP::Receiver sReceiver(sRecvConfig, sAsio, [sStage = 0](std::string&& aData) mutable {
        sStage++;
        switch (sStage)
        {
        case 1: throw std::runtime_error("test error 1");
        case 2: BOOST_CHECK_EQUAL(aData, "test123456");
        }
    });
    MQ::UDP::Sender sSender(sSendConfig, sAsio);

    sSender.push("test123");
    sSender.push("456");
    Threads::sleep(1.1);
    BOOST_CHECK_EQUAL(sSender.size(), 1);   // first time is error, so entry should stay in queue
    Threads::sleep(6.1);
    BOOST_CHECK_EQUAL(sSender.size(), 0);   // entry processed

    sAsio.term();
    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()
