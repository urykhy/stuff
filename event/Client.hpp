#pragma once

#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>
#include "Error.hpp"

namespace Event
{
    class Client : boost::asio::coroutine, public std::enable_shared_from_this<Client>
    {
        using tcp = boost::asio::ip::tcp;

    public:
        using Result = std::promise<tcp::socket&>;
        using Handler = std::function<void(std::future<tcp::socket&>)>;

    private:
        boost::asio::io_service& m_Loop;
        tcp::socket              m_Socket;
        boost::asio::deadline_timer m_Timer;
        const tcp::endpoint& m_Addr;
        const unsigned m_TimeoutMs;
        Handler m_Handler;

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, unsigned aTimeoutMs, Handler aHandler)
        : m_Loop(aLoop)
        , m_Socket(aLoop)
        , m_Timer(aLoop)
        , m_Addr(aAddr)
        , m_TimeoutMs(aTimeoutMs)
        , m_Handler(aHandler)
        { }

        void start()
        {
            m_Timer.expires_from_now(boost::posix_time::millisec(m_TimeoutMs));
            m_Socket.async_connect(m_Addr, [this, p=this->shared_from_this()](boost::system::error_code error)
            {
                m_Timer.cancel();
                std::promise<tcp::socket&> sPromise;
                if (!error)
                {
                    sPromise.set_value(m_Socket);
                    m_Socket.set_option(tcp::no_delay(true));
                }
                else
                    sPromise.set_exception(std::make_exception_ptr(NetworkError(error)));
                m_Handler(sPromise.get_future());
            });
            m_Timer.async_wait([p=this->shared_from_this()](boost::system::error_code ec){
                if (!ec)
                    p->m_Socket.close();
            });
        }

        void stop() {
            // call close operation from asio thread only to avoid race
            m_Socket.get_io_service().post([p=this->shared_from_this()](){
                if (!p->is_open())
                    return;
                boost::system::error_code ec;   // ignore shutdown error
                p->m_Socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                p->m_Socket.close();
            });
        }
        bool is_open() const { return m_Socket.is_open(); }
    };

} // namespace Event