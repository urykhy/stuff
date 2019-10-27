#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Server.hpp"
#include "Client.hpp"
#include "Echo.hpp"
#include "Framed.hpp"
#include <threads/Asio.hpp>

class Message
{
    uint32_t size = 0;
    std::string data;
public:

    Message() {}
    Message(const std::string& aBody)
    : size(aBody.size())
    , data(aBody)
    { }

    struct Header {
        uint32_t size = 0;
        size_t decode() const { return size; }
    } __attribute__ ((packed));

    using const_buffer = boost::asio::const_buffer;
    using BufferList = std::array<const_buffer, 2>;
    BufferList    as_buffer()     const { return BufferList({header_buffer(), body_buffer()}); }

private:
    const_buffer  header_buffer() const { return boost::asio::buffer((const void*)&size, sizeof(size)); }
    const_buffer  body_buffer()   const { return boost::asio::buffer((const void*)&data[0], data.size()); }
};

BOOST_AUTO_TEST_SUITE(Event)
BOOST_AUTO_TEST_CASE(echo)
{
    Threads::Group sGroup;
    Threads::Asio  sLoop;
    sLoop.start(1, sGroup);

    auto sServer = std::make_shared<Event::Server>(sLoop.service());
    sServer->start(1234, [](std::shared_ptr<boost::asio::ip::tcp::socket>&& aSocket)
    {
        BOOST_CHECK_MESSAGE(true, "connection from " << aSocket->remote_endpoint());
        auto sTmp = std::make_shared<Event::Echo::Server>(std::move(aSocket));
        sTmp->start();
    });

    auto sClient = std::make_shared<Event::Client>(sLoop.service());
    sClient->start(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 1234), 100, [](std::future<boost::asio::ip::tcp::socket&> aSocket)
    {
        try {
            auto& sSocket = aSocket.get();
            BOOST_CHECK_MESSAGE(true, "connected to " << sSocket.remote_endpoint());
            auto sTmp = std::make_shared<Event::Echo::Client>(sSocket);
            sTmp->start();
        } catch (const std::exception& e) {
            BOOST_CHECK_MESSAGE(false, "fail to connect: " << e.what());
        }
    });

    sleep (1);
    sGroup.wait();
}
BOOST_AUTO_TEST_CASE(framed)
{
    Threads::Group sGroup;
    Threads::Asio  sLoop;
    sLoop.start(1, sGroup);

    auto sServer = std::make_shared<Event::Server>(sLoop.service());
    sServer->start(1234, [](std::shared_ptr<boost::asio::ip::tcp::socket>&& aSocket)
    {
        BOOST_CHECK_MESSAGE(true, "connection from " << aSocket->remote_endpoint());
        auto sTmp = std::make_shared<Event::Framed::Server<Message>>(std::move(aSocket), [](std::string& arg) -> std::string {
            if (arg == "normal")
                return "OK";
            else
                return "ERROR";
        });
        sTmp->start();
    });

    int sClientCalls = 0;
    auto sClient = std::make_shared<Event::Client>(sLoop.service());
    sClient->start(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 1234), 100, [&sClientCalls](std::future<boost::asio::ip::tcp::socket&> aSocket)
    {
        try {
            auto& sSocket = aSocket.get();
            BOOST_CHECK_MESSAGE(true, "connected to " << sSocket.remote_endpoint());
            auto sTmp = std::make_shared<Event::Framed::Client<Message>>(sSocket, [&sClientCalls](std::future<std::string>&& aResult){
                sClientCalls++;
                switch (sClientCalls)
                {
                case 1: BOOST_CHECK_EQUAL(aResult.get(), "OK");    break;
                case 2: BOOST_CHECK_EQUAL(aResult.get(), "ERROR"); break;
                }
            });
            sTmp->start();
            sTmp->call("normal");
            sTmp->call("bad");
        } catch (const std::exception& e) {
            BOOST_CHECK_MESSAGE(false, "fail to connect: " << e.what());
        }
    });

    sleep (1);
    sGroup.wait();
    BOOST_CHECK_MESSAGE(sClientCalls == 2, "all client calls are made");
}
BOOST_AUTO_TEST_SUITE_END()
