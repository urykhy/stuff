#pragma once

#include <cassert>
#include <event/Client.hpp>
#include <event/Server.hpp>
#include <event/Framed.hpp>

#include "Library.hpp"
#include "ReplyWaiter.hpp"

namespace RPC
{
    class Message
    {
        uint32_t size = 0;  // in network order
        std::string data;
    public:

        Message() {}
        Message(const std::string& aBody)
        : size(htonl(aBody.size()))
        , data(aBody)
        { }
        uint32_t size_() const { return ntohl(size); }

        struct Header {
            uint32_t size = 0;
            size_t decode() const { return ntohl(size); }
        } __attribute__ ((packed));

        using const_buffer = boost::asio::const_buffer;
        using BufferList = std::array<const_buffer, 2>;
        BufferList    as_buffer()     const { return BufferList({header_buffer(), body_buffer()}); }

    private:
        const_buffer  header_buffer() const { return boost::asio::buffer((const void*)&size, sizeof(size)); }
        const_buffer  body_buffer()   const { return boost::asio::buffer((const void*)&data[0], data.size()); }
    };

    struct Client : public std::enable_shared_from_this<Client>
    {
        using Result = std::string;
        using Handler = std::function<void(std::future<std::string>&&)>;

    private:
        static constexpr unsigned CONNECT_TIMEOUT = 100;
        using tcp = boost::asio::ip::tcp;
        using Transport= Event::Framed::Client<Message>;

        std::shared_ptr<Event::Client> m_Client;
        std::shared_ptr<Transport> m_Transport;

        boost::asio::io_service& m_Loop;
        std::shared_ptr<ReplyWaiter> m_Queue;
        const tcp::endpoint m_Addr;
        std::atomic<uint64_t> m_Serial{1};

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr)
        : m_Loop(aLoop)
        , m_Queue(std::make_shared<ReplyWaiter>(aLoop))
        , m_Addr(aAddr)
        {}

        void start()
        {
            m_Client = std::make_shared<Event::Client>(m_Loop);
            m_Client->start(m_Addr, CONNECT_TIMEOUT, [this, p=this->shared_from_this()](std::future<tcp::socket&> aSocket)
            {
                auto& sSocket = aSocket.get(); // FIXME: handle exception
                m_Transport = std::make_shared<Transport>(sSocket, [this, p](std::future<std::string>&& aResult){
                    std::promise<std::string> sPromise;
                    auto sSerial = Library::parseResponse(aResult, sPromise);
                    if (sSerial > 0)
                        m_Queue->call(sSerial, sPromise.get_future());
                });
                m_Transport->start();
                m_Queue->start();
            });
        }

        void call(const std::string& aName, const std::string& aArgs, Handler aHandler, unsigned aTimeoutMs = 100)
        {
            const uint64_t sSerial = m_Serial++;
            m_Queue->insert(sSerial, aTimeoutMs, aHandler);
            m_Transport->call(Library::formatCall(sSerial, aName, aArgs));
        }

        void stop() { if (m_Queue) m_Queue->stop(); }
    };

    class Server : public std::enable_shared_from_this<Server>
    {
        using tcp = boost::asio::ip::tcp;
        using Transport = Event::Framed::Server<Message>;
        boost::asio::io_service& m_Loop;
        const uint16_t m_Port;

        std::shared_ptr<Event::Server> m_Server;
        Library m_Library;

    public:
        Server(boost::asio::io_service& aLoop, uint16_t aPort)
        : m_Loop(aLoop)
        , m_Port(aPort)
        { }

        void start()
        {
            m_Server = std::make_shared<Event::Server>(m_Loop);
            m_Server->start(m_Port, [this, p=this->shared_from_this()](std::shared_ptr<tcp::socket>&& aSocket)
            {
                auto sTmp = std::make_shared<Transport>(std::move(aSocket), [this, p](std::string& arg) -> std::string {
                    return m_Library.call(arg);
                });
                sTmp->start();
            });
        }

        void insert(const std::string& aName, Library::Handler aHandler)
        {
            // FIXME: locking
            m_Library.insert(aName, aHandler);
        }

        void stop() {
            if (m_Server) m_Server->stop();
        }
    };

} // namespace RPC