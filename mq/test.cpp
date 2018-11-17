#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include <MessageQueue.hpp>
#include <WorkQ.hpp>
#include <Periodic.hpp> // for sleep

struct DummySender : MQ::SenderTransport
{
    MQ::SenderRecv* m_Back = nullptr;
    size_t m_Action = 0;
    void push(size_t sSerial, const std::string& sData) override
    {
        m_Action++;
        switch (m_Action)
        {
        case 1:
            BOOST_CHECK_EQUAL(sSerial, 1);
            BOOST_CHECK_EQUAL(sData, "test123");
            m_Back->ack(sSerial);
            break;
        case 2:
            m_Back->restore();
            break;
        case 3:
            BOOST_CHECK_EQUAL(sSerial, 2);
            BOOST_CHECK_EQUAL(sData, "test456test789");
            m_Back->ack(sSerial);
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
    MQ::Sender sSender(sAsio, &sDummy);
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

    MQ::Receiver sRecv(sHandler);
    sRecv.push(1, "test123");
    sRecv.push(1, "test123");
    sRecv.push(2, "test456");
    BOOST_CHECK_EQUAL(sRecv.size(), 2);
}
BOOST_AUTO_TEST_SUITE_END()
