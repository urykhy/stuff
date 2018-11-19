#pragma once

#include <iostream>
#include <MessageQueue.hpp>

namespace MQ::UDP
{
    using udp = asio::ip::udp;

    struct Config
    {
        std::string host;
        uint16_t    port = 0;

        udp::endpoint resolve() const
        {
            return udp::endpoint(asio::ip::address::from_string(host), port);
        }
    };

    struct Header
    {
        uint64_t serial = 0;
        uint64_t crc    = 0;
        uint16_t size   = 0;
    };

    struct Reply
    {
        uint64_t serial = 0;
    };

    class Sender : public SenderTransport
    {
        const udp::endpoint m_Endpoint;
        udp::socket m_Socket;
        aux::Sender m_Sender;

        std::string m_Buffer;
        udp::endpoint m_Remote;
        const std::hash<std::string> m_Hash{};

        void push(size_t aSerial, const std::string& aBody) override
        {
            const Header sHeader{aSerial, m_Hash(aBody), (uint16_t)aBody.size()};
            std::array<asio::const_buffer, 2> sBuffer;
            sBuffer[0] = asio::buffer(&sHeader, sizeof(sHeader));
            sBuffer[1] = asio::buffer(aBody);
            //std::cerr << "send task " << sHeader.serial << " with " << sHeader.size << " bytes" << std::endl;
            m_Socket.send_to(sBuffer, m_Endpoint);
        }

        void start()
        {
            m_Socket.async_receive_from(boost::asio::buffer(m_Buffer)
                                      , m_Remote
                                      , [this](const boost::system::error_code& aError, std::size_t aBytes) {
                                            if (!aError)
                                                this->cb(aBytes);
                                        });
        }

        void cb(size_t aSize)
        {
            assert(aSize == sizeof(Reply));
            const Reply* sReply = reinterpret_cast<const Reply*>(m_Buffer.data());
            //std::cerr << "got ack for " << sReply->serial << std::endl;
            m_Sender.ack(sReply->serial);
            start();
        }

    public:
        Sender(const Config& aConfig, Threads::WorkQ& aWorkQ)
        : m_Endpoint(aConfig.resolve())
        , m_Socket(aWorkQ.service())
        , m_Sender(aWorkQ, this)
        {
            m_Buffer.resize(sizeof(Header));
            m_Socket.open(udp::v4());
            start();
        }

        void push(const std::string& aBody) { m_Sender.push(aBody); }
        size_t size() const { return m_Sender.size(); }
    };

    class Receiver
    {
        aux::Receiver m_Receiver;
        udp::socket m_Socket;
        udp::endpoint m_Remote;
        std::string m_Buffer;
        const std::hash<std::string> m_Hash{};

        void start()
        {
            m_Socket.async_receive_from(boost::asio::buffer(m_Buffer)
                                      , m_Remote
                                      , [this](const boost::system::error_code& aError, std::size_t aBytes) {
                                            if (!aError)
                                                this->cb(aBytes);
                                        });
        }

        void cb(std::size_t aSize)
        {
            assert(aSize > sizeof(Header));
            const Header* sHeader = reinterpret_cast<const Header*>(m_Buffer.data());
            assert(sHeader->size + sizeof(Header) == aSize);
            std::string sBody = m_Buffer.substr(sizeof(Header), sHeader->size); // FIXME: pass string_vew to user ?
            assert(m_Hash(sBody) == sHeader->crc);

            //std::cerr << "got  task " << sHeader->serial << " with " << sHeader->size << " bytes" << std::endl;

            try
            {
                m_Receiver.push(sHeader->serial, std::move(sBody));
                const Reply sReply{sHeader->serial};
                m_Socket.send_to(asio::buffer(&sReply, sizeof(sReply)), m_Remote);
            }
            catch (const std::exception& e)
            {   // FIXME: send kind ok NAK packet ?
                std::cerr << "UDP::Receiver exception: " << e.what();
            }
            start();
        }

    public:
        Receiver(const Config& aConfig, Threads::WorkQ& aWorkQ, aux::Handler aHandler)
        : m_Receiver(std::move(aHandler))
        , m_Socket(aWorkQ.service(), aConfig.resolve())
        {
            m_Buffer.resize(MAX_BYTES);
            start();
        }
    };
}
