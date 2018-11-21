#pragma once

#include <iostream>
#include <MessageQueue.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace MQ::UDP
{
    using namespace boost::multi_index;
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

    // support only one threaded io_service
    // cause one thread can call push, and other fire on timer
    class Sender : public SenderTransport
    {
        const udp::endpoint m_Endpoint;
        udp::socket m_Socket;
        aux::Sender m_Sender;

        std::string m_Buffer;
        udp::endpoint m_Remote;
        const std::hash<std::string> m_Hash{};

        enum { RETRY_SEC = 5, EXPIRE_BY_TIME = 0, EXPIRE_BY_SERIAL = 1, };

        struct Expire
        {
            time_t timeout = 0;
            size_t serial  = 0;

            Expire() {}
            Expire(time_t aTimeout, size_t aSerial) : timeout(aTimeout), serial(aSerial) {}

            struct _time {};
            struct _serial {};
        };

        using XStore = boost::multi_index_container<Expire,
            indexed_by<
                ordered_non_unique<
                    tag<typename Expire::_time>, member<Expire, time_t, &Expire::timeout>
                >,
                hashed_unique<
                    tag<typename Expire::_serial>, member<Expire, size_t, &Expire::serial>
                >
            >
        >;
        XStore m_ExpireState;
        boost::asio::deadline_timer m_RetryTimer;

        void push(size_t aSerial, const std::string& aBody) override
        {
            const Header sHeader{aSerial, m_Hash(aBody), (uint16_t)aBody.size()};
            std::array<asio::const_buffer, 2> sBuffer;
            sBuffer[0] = asio::buffer(&sHeader, sizeof(sHeader));
            sBuffer[1] = asio::buffer(aBody);
            std::cerr << "send task " << sHeader.serial << " with " << sHeader.size << " bytes" << std::endl;
            m_Socket.send_to(sBuffer, m_Endpoint);
			m_ExpireState.insert(Expire(time(nullptr) + RETRY_SEC, aSerial));
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
            std::cerr << "got ack for " << sReply->serial << std::endl;
            m_Sender.ack(sReply->serial);
            get<EXPIRE_BY_SERIAL>(m_ExpireState).erase(sReply->serial);
            start();
        }

        void set_timer()
        {
            m_RetryTimer.expires_from_now(boost::posix_time::seconds(1));
            m_RetryTimer.async_wait([this](auto error){ if (!error) retry_proc(); });
        }

        void retry_proc()
        {

            const time_t sDeadline = ::time(nullptr);
            for (const auto& x : get<EXPIRE_BY_TIME>(m_ExpireState))
                if (x.timeout < sDeadline)
                {
                    std::cerr << "retry for " << x.serial << std::endl;
                    m_Sender.restore(x.serial);
                }
                else
                    break;

            set_timer();
        }

    public:
        Sender(const Config& aConfig, Threads::WorkQ& aWorkQ)
        : m_Endpoint(aConfig.resolve())
        , m_Socket(aWorkQ.service())
        , m_Sender(aWorkQ, this)
        , m_RetryTimer(aWorkQ.service())
        {
            m_Buffer.resize(sizeof(Header));
            m_Socket.open(udp::v4());
            start();
            set_timer();
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

            std::cerr << "got  task " << sHeader->serial << " with " << sHeader->size << " bytes" << std::endl;

            try
            {
                const aux::TaskSerial sSerial{sHeader->serial, m_Remote.address().to_v4().to_uint(), m_Remote.port()};
                m_Receiver.push(sSerial, std::move(sBody));
                const Reply sReply{sHeader->serial};
                m_Socket.send_to(asio::buffer(&sReply, sizeof(sReply)), m_Remote);
            }
            catch (const std::exception& e)
            {
                std::cerr << "UDP::Receiver exception: " << e.what() << std::endl;
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
