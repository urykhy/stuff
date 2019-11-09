#pragma once

#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>

namespace tnt17
{
    class Auth : boost::asio::coroutine, public std::enable_shared_from_this<Auth>
    {
        using tcp = boost::asio::ip::tcp;
        using Handler = std::function<void(boost::system::error_code ec)>;
        const unsigned TIMEOUT_MS=100;

        tcp::socket& m_Socket;
        Handler      m_Handler;
        boost::asio::deadline_timer m_Timer;

        struct Greetings
        {
            char version[64];
            char salt[44];
            char dummy[20];
        } __attribute__((packed));
        Greetings m_Greetings;

        // boost::system::errc::make_error_code(boost::system::errc::permission_denied)
        // but it's a RemoteError, not NetworkError
        void timeout_func()
        {
            xcall(boost::system::errc::make_error_code(boost::system::errc::timed_out));
        }

        void xcall(boost::system::error_code ec)
        {
            if (m_Handler)
                m_Socket.get_io_service().post([m_Handler = std::move(m_Handler), ec]() { m_Handler(ec); });
        }

    public:
        Auth(tcp::socket& aSocket, Handler aHandler)
        : m_Socket(aSocket)
        , m_Handler(aHandler)
        , m_Timer(aSocket.get_io_service())
        {
            m_Timer.expires_from_now(boost::posix_time::millisec(TIMEOUT_MS));
            m_Timer.async_wait([this](auto error){ if (!error) this->timeout_func(); });
        }
        void start() { operator()(); }
#include <boost/asio/yield.hpp>
        void operator()(boost::system::error_code ec = boost::system::error_code(), size_t size = 0)
        {
            if (ec) { xcall(ec); return; }
            reenter (this)
            {
                yield boost::asio::async_read(m_Socket, boost::asio::buffer(&m_Greetings, sizeof(m_Greetings)), [p=shared_from_this()](boost::system::error_code ec, size_t size){
                    (*p)(ec, size);
                });
                //BOOST_TEST_MESSAGE("greeting from " << boost::string_ref(m_Greetings.version, 64));
                xcall(ec);
            }
        }
#include <boost/asio/unyield.hpp>
    };
}