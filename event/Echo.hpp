#pragma once

namespace Event::Echo
{
    // echo server and client
    class Client : boost::asio::coroutine, public std::enable_shared_from_this<Client>
    {
        using tcp = boost::asio::ip::tcp;
        tcp::socket& m_Socket;
        const std::string m_Data="1234567890";
        std::string m_Buffer;

    public:
        Client(tcp::socket& aSocket)
        : m_Socket(aSocket)
        , m_Buffer(m_Data.size(), ' ')
        {}

        void start() {
            operator()();
        }

#include <boost/asio/yield.hpp>
        void operator()(boost::system::error_code ec = boost::system::error_code(), size_t size = 0)
        {
            if (ec)
                return;
            reenter (this)
            {
                yield boost::asio::async_write(m_Socket, boost::asio::buffer(m_Data), [p=shared_from_this()](boost::system::error_code ec, size_t size){
                    (*p)(ec, size);
                });
                yield boost::asio::async_read(m_Socket, boost::asio::buffer(m_Buffer), [p=shared_from_this()](boost::system::error_code ec, size_t size){
                    (*p)(ec, size);
                });
                m_Socket.close();
            }
        }
#include <boost/asio/unyield.hpp>

        ~Client()
        {
            BOOST_CHECK_EQUAL(m_Data, m_Buffer);
        }
    };

    class Server : boost::asio::coroutine, public std::enable_shared_from_this<Server>
    {
        using tcp = boost::asio::ip::tcp;
        std::shared_ptr<tcp::socket> m_Socket;
        std::string m_Buffer;
    public:

        Server(std::shared_ptr<tcp::socket>&& aSocket)
        : m_Socket(aSocket)
        , m_Buffer(12, ' ')
        {}

        void start()
        {
            operator()();
        }

#include <boost/asio/yield.hpp>
        void operator()(boost::system::error_code ec = boost::system::error_code(), size_t size = 0)
        {
            if (ec)
                return;
            reenter (this)
            {
                yield m_Socket->async_read_some(boost::asio::buffer(m_Buffer), [p=shared_from_this()](boost::system::error_code ec, size_t size){
                    (*p)(ec, size);
                });
                yield boost::asio::async_write(*m_Socket, boost::asio::buffer(m_Buffer.data(), size), [p=shared_from_this()](boost::system::error_code ec, size_t size){
                    (*p)(ec, size);
                });
            }
        }
#include <boost/asio/unyield.hpp>
    };
} // namespace Event::Echo