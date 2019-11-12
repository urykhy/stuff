#pragma once

#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>

namespace Event {

    class Server : boost::asio::coroutine, public std::enable_shared_from_this<Server>
    {
    public:
        using tcp = boost::asio::ip::tcp;
        using Handler = std::function<void(std::shared_ptr<tcp::socket>&&)>;

    private:
        boost::asio::io_service& m_Loop;
        std::shared_ptr<tcp::acceptor> m_Acceptor;
        Handler m_Handler;
        std::shared_ptr<tcp::socket> m_Socket;

    public:

        Server(boost::asio::io_service& aLoop)
        : m_Loop(aLoop)
        { }

        void start(uint16_t aPort, Handler aHandler)
        {
            m_Handler = aHandler;
            boost::asio::ip::tcp::endpoint aAddr(boost::asio::ip::tcp::v4(), aPort);
            m_Acceptor = std::make_shared<tcp::acceptor>(m_Loop, aAddr);

            operator()();   // launch coroutine
        }

        void stop() {
            if (m_Acceptor) m_Acceptor->close();
        }

#include <boost/asio/yield.hpp>
        void operator()(boost::system::error_code ec = boost::system::error_code())
        {
            if (ec)
                return;
            reenter (this)
            {
                while (true)
                {
                    m_Socket = std::make_shared<tcp::socket>(m_Loop);
                    yield m_Acceptor->async_accept(*m_Socket, [p=shared_from_this()](auto error){ p->operator()(error); });
                    m_Socket->set_option(tcp::no_delay(true));
                    m_Handler(std::move(m_Socket));
                };
            }
         }
#include <boost/asio/unyield.hpp>
    };
} // namespace Event