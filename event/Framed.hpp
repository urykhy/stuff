#pragma once

#include "Client.hpp"
#include "Server.hpp"

namespace Event::Framed
{
    // framed server and client
    // FIXME: must use 1 threaded WorkQ, no locks/strand around queues
    template<class Message>
    class Client : public std::enable_shared_from_this<Client<Message>>
    {
    public:
        using Response = std::promise<std::string>;
        using Handler = std::function<void(std::future<std::string>&&)>;

    private:
        using tcp = boost::asio::ip::tcp;
        tcp::socket& m_Socket;
        Handler      m_Handler;

        boost::asio::coroutine m_Writer;
        std::list<Message>     m_Queue;
        boost::asio::coroutine m_Reader;

        typename Message::Header m_ReplyHeader;
        std::string              m_ReplyData;

    public:
        Client(tcp::socket& aSocket, Handler aHandler)
        : m_Socket(aSocket)
        , m_Handler(aHandler)
        { }

        void start() {
            reader();
        }

        void call(const std::string& aRequest)
        {
            m_Socket.get_io_service().post([this, p=this->shared_from_this(), aRequest] () mutable {
                const bool sWriteOut = m_Queue.empty();
                m_Queue.emplace_back(Message(aRequest));
                if (sWriteOut)
                    writer();
            });
        }

    private:
#include <boost/asio/yield.hpp>
        void writer(boost::system::error_code ec = boost::system::error_code(), size_t size = 0)
        {
            if (ec)
                return;

            reenter (&m_Writer)
            {
                while (!m_Queue.empty())
                {
                    yield boost::asio::async_write(m_Socket, m_Queue.front().as_buffer(), resume_writer());
                    m_Queue.pop_front();
                }
            }
        }
        std::function<void(boost::system::error_code ec, size_t size)> resume_writer() {
            return [p=this->shared_from_this()](boost::system::error_code ec, size_t size){ p->writer(ec, size); };
        }

        void reader(boost::system::error_code ec = boost::system::error_code(), size_t size = 0)
        {
            if (ec)
            {
                callback(ec);
                return;
            }

            reenter (&m_Reader)
            {
                while (true)
                {
                    yield boost::asio::async_read(m_Socket, boost::asio::buffer(&m_ReplyHeader, sizeof(m_ReplyHeader)), resume_reader());
                    m_ReplyData.resize(m_ReplyHeader.decode());
                    yield boost::asio::async_read(m_Socket, boost::asio::buffer(m_ReplyData), resume_reader());
                    callback(m_ReplyData);
                }
            }
        }
        std::function<void(boost::system::error_code ec, size_t size)> resume_reader() {
            return [p=this->shared_from_this()](boost::system::error_code ec, size_t size){ p->reader(ec, size); };
        }
#include <boost/asio/unyield.hpp>

        void callback(std::string& aReply)
        {
            std::promise<std::string> sPromise;
            sPromise.set_value(aReply);
            m_Handler(sPromise.get_future());
        }
        void callback(boost::system::error_code ec)
        {
            std::promise<std::string> sPromise;
            sPromise.set_exception(std::make_exception_ptr(NetworkError(ec)));
            m_Handler(sPromise.get_future());
            m_Queue.clear();
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
        std::function<void(boost::system::error_code ec, size_t size)> resume() {
            return [p=this->shared_from_this()](boost::system::error_code ec, size_t size){ (*p)(ec, size); };
        }
#include <boost/asio/unyield.hpp>
    };
} // namespace Event::Framed