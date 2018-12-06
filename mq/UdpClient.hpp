#pragma once

#include <UdpInternal.hpp>

namespace MQ::UDP
{
    class Client
    {
        const udp::endpoint m_Endpoint;
        aux::Receiver m_Receiver;
        udp::socket m_Socket;
        std::string m_Buffer;
        const std::hash<std::string> m_Hash{};
        std::atomic_bool m_Stop{false};
        boost::asio::deadline_timer m_HelloTimer;

        void start()
        {
            m_Socket.async_receive(boost::asio::buffer(m_Buffer)
                                  , [this](const boost::system::error_code& aError, std::size_t aBytes) {
                                        if (!aError)
                                            this->cb(aBytes);
                                    });
        }

        void cb(std::size_t aSize)
        {
            assert(aSize >= sizeof(Header));
            const Header* sHeader = reinterpret_cast<const Header*>(m_Buffer.data());
            assert(sHeader->size + sizeof(Header) == aSize);

            // FIXME: can out of order packets corrupt exit sequence ?
            // m_Remote.address().to_v4().to_uint(), m_Remote.port()
            try
            {
                if (sHeader->size > 0)
                {
                    std::cerr << "Client: recv task " << sHeader->serial << " with " << sHeader->size << " bytes" << std::endl;
                    std::string sBody = m_Buffer.substr(sizeof(Header), sHeader->size); // FIXME: pass string_vew to user ?
                    assert(m_Hash(sBody) == sHeader->crc);
                    m_Receiver.push(sHeader->serial, std::move(sBody));
                    const Reply sReply{sHeader->serial, flag(m_Stop)};
                    m_Socket.send_to(asio::buffer(&sReply, sizeof(sReply)), m_Endpoint);
                }
                else
                {
                    std::cerr << "Client: recv heartbeat with first task " << sHeader->min_serial << std::endl;
                }
                m_Receiver.clear(sHeader->min_serial);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Client: exception: " << e.what() << std::endl;
            }
            start();
        }

        void set_timer()
        {
            m_HelloTimer.expires_from_now(boost::posix_time::seconds(1));
            m_HelloTimer.async_wait([this](auto error){ if (!error) hello_proc(); });
        }

        void hello_proc()
        {
            const Reply sReply{0, flag(m_Stop)};
            m_Socket.send_to(asio::buffer(&sReply, sizeof(sReply)), m_Endpoint);

            std::cerr << "Client: send hello" << std::endl;

            set_timer();
        }

    public:
        Client(const Config& aConfig, Threads::WorkQ& aWorkQ, aux::Handler aHandler)
        : m_Endpoint(aConfig.peer())
        , m_Receiver(std::move(aHandler))
        , m_Socket(aWorkQ.service(), aConfig.bind())
        , m_HelloTimer(aWorkQ.service())
        {
            std::cerr << "Client: bind to " <<  aConfig.bind() << std::endl;
            m_Buffer.resize(MAX_BYTES);
            set_timer();
            start();
        }

        ~Client()
        {
            wait();
        }

        void wait()
        {
            std::cerr << "Client: wait" << std::endl;
            using namespace std::chrono_literals;
            m_Stop = true;
            while (!m_Receiver.empty())
                std::this_thread::sleep_for(0.1s);
            std::cerr << "Client: stopped" << std::endl;
        }
    };
}
