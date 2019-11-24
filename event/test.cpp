#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include <threads/Asio.hpp>
#include "Framed.hpp"
#include "Echo.hpp"

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
#if 0
BOOST_AUTO_TEST_CASE(echo)
{
    Threads::Group sGroup;
    Threads::Asio  sLoop;
    sLoop.start(1, sGroup);

    auto sServer = std::make_shared<Event::Server>(sLoop.service(), 1234, [](std::shared_ptr<boost::asio::ip::tcp::socket>&& aSocket)
    {
        BOOST_CHECK_MESSAGE(true, "connection from " << aSocket->remote_endpoint());
        auto sTmp = std::make_shared<Event::Echo::Server>(std::move(aSocket));
        sTmp->start();
    });
    sServer->start();

    auto sClient = std::make_shared<Event::Client>(sLoop.service(), boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 1234), 100, [](std::future<Event::Client::Ptr> aClient)
    {
        try {
            auto sClient = aClient.get();
            auto& sSocket = sClient->socket();
            BOOST_CHECK_MESSAGE(true, "connected to " << sSocket.remote_endpoint());
            auto sTmp = std::make_shared<Event::Echo::Client>(sSocket);
            sTmp->start();
        } catch (const std::exception& e) {
            BOOST_CHECK_MESSAGE(false, "fail to connect: " << e.what());
        }
    });
    sClient->start();

    sleep (1);
    sGroup.wait();
}
#endif
BOOST_AUTO_TEST_CASE(framed)
{
    Threads::Group sGroup;
    Threads::Asio  sLoop;
    sLoop.start(4, sGroup);

    auto sServer = std::make_shared<Event::Server>(sLoop.service(), 1234, [](std::shared_ptr<boost::asio::ip::tcp::socket>&& aSocket)
    {
        BOOST_CHECK_MESSAGE(true, "connection from " << aSocket->remote_endpoint());
        auto sTmp = std::make_shared<Event::Framed::Server<Message>>(std::move(aSocket), [](std::string& arg) -> std::string {
            if (arg == "normal")
                return arg + " OK";
            else
                return arg + " ERROR";
        });
        sTmp->start();
    });
    sServer->start();

    std::atomic_int sClientCalls{0};
    auto sClient = std::make_shared<Event::Client>(sLoop.service(), Event::endpoint("127.0.0.1", 1234), 100, [&sClientCalls](std::future<Event::Client::Ptr> aClient)
    {
        try
        {
            auto sClient = aClient.get();
            auto& sSocket = sClient->socket();
            BOOST_CHECK_MESSAGE(true, "connected to " << sSocket.remote_endpoint());
            auto sTmp = std::make_shared<Event::Framed::Client<Message>>(sClient, [&sClientCalls](std::future<std::string>&& aResult)
            {
                try{
                    const std::string sResult = aResult.get();
                    sClientCalls++;
                    BOOST_CHECK(sResult == "normal OK" or sResult == "bad ERROR");
                } catch (const std::exception& e)  {
                    BOOST_TEST_MESSAGE("connection closed: " << e.what());
                }
            });
            sTmp->start();
            sTmp->call("normal");
            sTmp->call("bad");
        } catch (const std::exception& e) {
            BOOST_CHECK_MESSAGE(false, "fail to connect: " << e.what());
        }
    });
    sClient->start();
    std::this_thread::sleep_for(200ms);
    sClient->stop();
    sClient->stop();
    std::this_thread::sleep_for(100ms);
    sGroup.wait();
    BOOST_CHECK_MESSAGE(sClientCalls == 2, "all client calls are made");
}
BOOST_AUTO_TEST_SUITE_END()
