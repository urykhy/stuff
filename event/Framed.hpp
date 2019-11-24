#pragma once

#include "Client.hpp"
#include "Server.hpp"
#include "ReplyWaiter.hpp"

namespace Event::Framed
{
    // framed server and client
    template<class Message>
    class Client : public std::enable_shared_from_this<Client<Message>>
    {
    public:
        using Response = std::promise<std::string>;
        using Handler = std::function<void(std::future<std::string>&&)>;

    private:
        ba::io_context::strand m_Strand;
        ba::deadline_timer     m_Timer;
        tcp::socket            m_Socket;
        const tcp::endpoint    m_Addr;

        State   m_State;
        Handler m_Handler;

        ba::coroutine m_Writer;
        ba::coroutine m_Reader;
        std::list<Message> m_Queue;
        ReplyWaiter   m_Waiter;

        typename Message::Header m_ReplyHeader;
        std::string              m_ReplyData;

        // wrap any operation with this socket
        template<class T>
        auto wrap(T&& t) -> decltype(m_Strand.wrap(std::move(t)))
        {
            return m_Strand.wrap(std::move(t));
        }

        template<class T>
        void post(T&& t)
        {
            m_Strand.post(std::move(t));
        }

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, Handler aHandler)
        : m_Strand(aLoop)
        , m_Timer(aLoop)
        , m_Socket(aLoop)
        , m_Addr(aAddr)
        , m_Handler(aHandler)
        { }

        void start() {
            reader();
        }

        bool call(size_t aSerial, const std::string& aRequest)
        {
            const unsigned sTimeoutMs = 100;
            if (!m_State.is_alive()))
                return false;

            post([this, p=this->shared_from_this(), aRequest] () mutable
            {
                const bool sWriteOut = m_Queue.empty();
                m_Queue.emplace_back(Message(aRequest));
                m_Waiter.insert(aSerial, sTimeoutMs, aHandler);
                if (sWriteOut)
                    writer();
            });
        }

    private:
#include <boost/asio/yield.hpp>
        void writer(boost::system::error_code ec = boost::system::error_code(), size_t size = 0)
        {
            if (ec || !m_State.is_alive())
            {
                m_Queue.clear();
                return;
            }

            reenter (&m_Writer)
            {
                while (!m_Queue.empty())
                {
                    yield boost::asio::async_write(m_Socket, m_Queue.front().as_buffer(), resume_writer());
                    m_Queue.pop_front();
                }
            }
        }
        auto resume_writer() {
            return wrap([p=this->shared_from_this()](boost::system::error_code ec, size_t size){ p->writer(ec, size); });
        }

        void reader(boost::system::error_code ec = boost::system::error_code(), size_t size = 0)
        {
            if (ec)
            {
                m_State.set_error();
                callback(ec);
                return;
            }

            reenter (&m_Reader)
            {
                m_State.connecting();
                m_Timer.expires_from_now(boost::posix_time::millisec(100)); // 100 ms to connect
                m_Timer.async_wait(wrap([p=this->shared_from_this()](boost::system::error_code ec){
                    if (!ec) p->m_Socket.close();
                }));
                yield m_Socket.async_connect(m_Addr, resume_reader());
                m_Timer.cancel();
                m_State.established();
                // FIXME: Auth

                set_timer();

                while (true)
                {
                    yield boost::asio::async_read(m_Socket, boost::asio::buffer(&m_ReplyHeader, sizeof(m_ReplyHeader)), resume_reader());
                    m_ReplyData.resize(m_ReplyHeader.decode());
                    yield boost::asio::async_read(m_Socket, boost::asio::buffer(m_ReplyData), resume_reader());
                    callback(m_ReplyData);
                }
            }
        }
        auto resume_reader() {
            return wrap([p=this->shared_from_this()](boost::system::error_code ec, size_t size){ p->reader(ec, size); });
        }
#include <boost/asio/unyield.hpp>

        void callback(std::string& aReply)
        {
            Header sHeader;
            Reply sReply;
            imemstream sStream(aReply);

            try {
                sHeader.parse(sStream);
                sReply.parse(sStream);
            } catch (const std::exception& e) {
                callback(boost::system::errc::make_error_code(boost::system::errc::protocol_error));
                notify(e.what());
                return;
            }

            std::promise<std::string> sPromise;
            if (sReply.ok) {
                const auto sRest = sStream.rest();
                std::string sRestStr(sRest.begin(), sRest.end());
                sPromise.set_value(sRestStr);
            } else {
                // normal error from server: not fatal, just a bad client call
                sPromise.set_exception(std::make_exception_ptr(Event::RemoteError(sReply.error)));
            }

            m_Queue.call(sHeader.sync, sPromise.get_future());

            //std::promise<std::string> sPromise;
            //sPromise.set_value(aReply);
            //m_Handler(sPromise.get_future());
        }

        // protocol error
        void notify(const std::string& aMsg)
        {
            std::promise<std::string> sPromise;
            sPromise.set_exception(std::make_exception_ptr(ProtocolError(aMsg)));
            m_Handler(sPromise.get_future());
        }

        // called on network error
        void callback(boost::system::error_code ec)
        {
            m_Client->set_error();
            std::promise<std::string> sPromise;
            sPromise.set_exception(std::make_exception_ptr(NetworkError(ec)));
            m_Handler(sPromise.get_future());
            m_Queue.clear();
        }
    private:

        void set_timer()
        {
            const unsigned TIMEOUT_MS = 1; // check timeouts every 1ms
            if (!m_State.is_running() and m_Queue.empty())
                return;
            m_Timer.expires_from_now(boost::posix_time::millisec(TIMEOUT_MS));
            m_Timer.async_wait(wrap([p=this->shared_from_this()](auto error)
            {
                if (!error)
                    p->timeout_func();
            }));
        }

        void timeout_func()
        {
            m_Queue.on_timer(); // report timeout on slow calls
            set_timer();
        }
    };

    template<class Message>
    class Server : boost::asio::coroutine, public std::enable_shared_from_this<Server<Message>>
    {
    public:
        using Handler = std::function<std::string(std::string&)>;

    private:
        using tcp = boost::asio::ip::tcp;
        std::shared_ptr<tcp::socket> m_Socket;
        Handler                      m_Handler;

        typename Message::Header m_RequestHeader;
        std::string              m_RequestData;
        Message                  m_Response;

    public:
        Server(std::shared_ptr<tcp::socket>&& aSocket, Handler aHandler)
        : m_Socket(aSocket)
        , m_Handler(aHandler)
        { }

        void start()
        {
            operator()();
        }

    private:
#include <boost/asio/yield.hpp>
        void operator()(boost::system::error_code ec = boost::system::error_code(), size_t size = 0)
        {
            if (ec)
                return;
            reenter (this)
            {
                while (true)
                {
                    yield boost::asio::async_read(*m_Socket, boost::asio::buffer(&m_RequestHeader, sizeof(m_RequestHeader)), resume());
                    m_RequestData.resize(m_RequestHeader.decode());
                    yield boost::asio::async_read(*m_Socket, boost::asio::buffer(m_RequestData), resume());
                    m_Response = Message(m_Handler(m_RequestData));
                    yield boost::asio::async_write(*m_Socket, m_Response.as_buffer(), resume());
                }
            }
        }
        auto resume() {
            return [p=this->shared_from_this()](boost::system::error_code ec, size_t size){ (*p)(ec, size); };
        }
#include <boost/asio/unyield.hpp>
    };
} // namespace Event::Framed