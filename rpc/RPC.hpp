#pragma once

#include <cassert>
#include <event/Client.hpp>
#include <event/Server.hpp>
#include <event/Framed.hpp>
#include "Library.hpp"

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

    struct Client
    {
        using Result = std::string;
        using Handler = std::function<void(std::future<std::string>&&)>;
        using Error = std::runtime_error;

    private:
        static constexpr unsigned CONNECT_TIMEOUT = 100;
        using tcp = boost::asio::ip::tcp;
        using Transport= Event::Framed::Client<Message>;

        std::shared_ptr<Event::Client> m_Client;
        std::shared_ptr<Transport> m_Transport;

        // server handle calls one by one, so we can store handlers in a list
        std::list<Handler> m_Queue; // FIXME: timeouts

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr)
        {
            m_Client = std::make_shared<Event::Client>(aLoop);
            m_Client->start(aAddr, CONNECT_TIMEOUT, [this](std::future<tcp::socket&> aSocket)
            {
                auto& sSocket = aSocket.get(); // FIXME: handle exception
                m_Transport = std::make_shared<Transport>(sSocket, [this](std::future<std::string>&& aResult){
                    std::promise<std::string> sPromise;
                    Library::parseResponse(aResult, sPromise);
                    assert(!m_Queue.empty());
                    m_Queue.front()(sPromise.get_future());
                    m_Queue.pop_front();
                });
                m_Transport->start();
            });
        }

        void call(const std::string& aName, const std::string& aArgs, Handler aHandler)
        {
            m_Queue.push_back(aHandler);    // FIXME: locking
            m_Transport->call(Library::formatCall(aName, aArgs));
        }
    };

    class Server
    {
        using tcp = boost::asio::ip::tcp;
        using Transport = Event::Framed::Server<Message>;
        std::shared_ptr<Event::Server> m_Server;
        Library m_Library;

    public:
        Server(boost::asio::io_service& aLoop, uint16_t aPort)
        {
            m_Server = std::make_shared<Event::Server>(aLoop);
            m_Server->start(aPort, [this](std::shared_ptr<tcp::socket>&& aSocket)
            {
                auto sTmp = std::make_shared<Transport>(std::move(aSocket), [this](std::string& arg) -> std::string {
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
    };

} // namespace RPC