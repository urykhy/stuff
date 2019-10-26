#pragma once

#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>

namespace Event
{
    class Client : boost::asio::coroutine, public std::enable_shared_from_this<Client>
    {
        using tcp = boost::asio::ip::tcp;

    public:
        using Result = std::promise<tcp::socket&>;
        using Handler = std::function<void(std::future<tcp::socket&>)>;
        using Error = std::runtime_error;

    private:
        boost::asio::io_service& m_Loop;
        tcp::socket              m_Socket;
        boost::asio::deadline_timer m_Timer;

    public:

        Client(boost::asio::io_service& aLoop)
        : m_Loop(aLoop)
        , m_Socket(aLoop)
        , m_Timer(aLoop)
        { ;; }

        void start(const tcp::endpoint& aAddr, unsigned aTimeoutMs, Handler aHandler)
        {
            m_Timer.expires_from_now(boost::posix_time::millisec(aTimeoutMs));
            m_Socket.async_connect(aAddr, [p=this->shared_from_this(), aHandler](auto error)
            {
                p->m_Timer.cancel();
                std::promise<tcp::socket&> sPromise;
                if (!error)
                    sPromise.set_value(p->m_Socket);
                else
                    sPromise.set_exception(std::make_exception_ptr(Error(error.message())));
                aHandler(sPromise.get_future());
            });
            m_Timer.async_wait([p=this->shared_from_this()](boost::system::error_code ec){
                if (!ec)
                    p->m_Socket.close();
            });
        }

        bool isOpen() const {
            return m_Socket.is_open();
        }
    };

} // namespace Event