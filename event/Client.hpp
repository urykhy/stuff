#pragma once

#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/strand.hpp>

#include "Error.hpp"
#include "State.hpp"

namespace Event
{
    namespace ba = boost::asio;
    using    tcp = ba::ip::tcp;

    tcp::endpoint endpoint(const std::string& aAddr, uint16_t aPort)
    {
        return tcp::endpoint(ba::ip::address::from_string(aAddr), aPort);
    }

    class Client : boost::asio::coroutine, public std::enable_shared_from_this<Client>
    {
    public:
        using Ptr = std::shared_ptr<Client>;
        using Result = std::promise<Ptr>;
        using Handler = std::function<void(std::future<Ptr>)>;

    private:
        ba::io_context::strand m_Strand;
        ba::deadline_timer     m_Timer;
        tcp::socket            m_Socket;
        const tcp::endpoint&   m_Addr;
        const unsigned         m_TimeoutMs;

        State   m_State;
        Handler m_Handler;

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, unsigned aTimeoutMs, Handler aHandler)
        : m_Strand(aLoop)
        , m_Timer(aLoop)
        , m_Socket(aLoop)
        , m_Addr(aAddr)
        , m_TimeoutMs(aTimeoutMs)
        , m_Handler(aHandler)
        { }

        void start()
        {
            m_State.connecting();
            m_Timer.expires_from_now(boost::posix_time::millisec(m_TimeoutMs));
            m_Socket.async_connect(m_Addr, wrap([this, p=this->shared_from_this()](boost::system::error_code error)
            {
                m_Timer.cancel();
                Result sPromise;
                if (!error)
                {
                    sPromise.set_value(p);
                    m_Socket.set_option(tcp::no_delay(true));
                    m_State.established();
                }
                else
                {
                    sPromise.set_exception(std::make_exception_ptr(NetworkError(error)));
                    m_State.set_error();
                }
                m_Handler(sPromise.get_future());
            }));
            m_Timer.async_wait(wrap([p=this->shared_from_this()](boost::system::error_code ec){
                if (!ec)
                    p->m_Socket.close();
            }));
        }

        void stop()
        {
            m_State.stop();
            m_State.close();

            // close socket from asio thread to avoid race
            post([p=this->shared_from_this()]()
            {
                if (!p->m_Socket.is_open()) return;
                boost::system::error_code ec;   // ignore shutdown error
                p->m_Socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                p->m_Socket.close();
            });
        }

        // wrap any operation with this socket
        template<class T>
        auto wrap(T&& t) -> decltype(m_Strand.wrap(std::move(t)))
        {
            return m_Strand.wrap(std::move(t));
        }

        template<class T>
        void post(T&& t)
        {
            m_Socket.get_io_service().post(wrap(std::move(t)));
        }

        tcp::socket& socket() { return m_Socket; }

        boost::asio::io_service& get_io_service()
        {
            return m_Socket.get_io_service();
        }

        bool is_running()   const { return m_State.is_running(); }
        bool is_connected() const { return m_State.is_connected(); }
        void set_error()          { m_State.close(); }
    };

} // namespace Event