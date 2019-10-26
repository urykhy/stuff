#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Suites
#include <boost/test/unit_test.hpp>

#include "Server.hpp"
#include "Client.hpp"
#include "Echo.hpp"
#include <threads/WorkQ.hpp>

BOOST_AUTO_TEST_SUITE(Event)
BOOST_AUTO_TEST_CASE(simple)
{
    Threads::Group sGroup;
    Threads::WorkQ sLoop;
    sLoop.start(1, sGroup);

    auto sServer = std::make_shared<Event::Server>(sLoop.service());
    sServer->start(1234, [](std::shared_ptr<boost::asio::ip::tcp::socket>&& aSocket)
    {
        BOOST_CHECK_MESSAGE(true, "connection from " << aSocket->remote_endpoint());
        auto sTmp = std::make_shared<Echo::Server>(std::move(aSocket));
        sTmp->start();
    });

    auto sClient = std::make_shared<Event::Client>(sLoop.service());
    sClient->start(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 1234), 100, [](std::future<boost::asio::ip::tcp::socket&> aSocket)
    {
        try {
            auto& sSocket = aSocket.get();
            BOOST_CHECK_MESSAGE(true, "connected to " << sSocket.remote_endpoint());
            auto sTmp = std::make_shared<Echo::Client>(sSocket);
            sTmp->start();
        } catch (const std::exception& e) {
            BOOST_CHECK_MESSAGE(false, "fail to connect: " << e.what());
        }
    });

    sleep (1);
    sGroup.wait();
}
BOOST_AUTO_TEST_SUITE_END()