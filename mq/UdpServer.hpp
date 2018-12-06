#pragma once

#include <UdpInternal.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace MQ::UDP
{
    using namespace boost::multi_index;

    // support only one threaded io_service
    // cause one thread can call push, and other fire on timer
    class Server : public SenderTransport
    {
        const udp::endpoint m_Endpoint;
        udp::socket m_Socket;
        aux::Sender m_Sender;

        std::string m_Buffer;
        const std::hash<std::string> m_Hash{};

        enum { RETRY_SEC = 1, EXPIRE_BY_TIME = 0, EXPIRE_BY_SERIAL = 1, };

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

        void push(size_t aSerial, const std::string& aBody, size_t aMinSerial) override
        {
            const Header sHeader{aMinSerial, aSerial, m_Hash(aBody), (uint16_t)aBody.size(), 0};
            std::array<asio::const_buffer, 2> sBuffer;
            sBuffer[0] = asio::buffer(&sHeader, sizeof(sHeader));
            sBuffer[1] = asio::buffer(aBody);
            std::cerr << "Server: send task " << sHeader.serial << " with " << sHeader.size << " bytes" << std::endl;
            m_Socket.send_to(sBuffer, m_Endpoint);
			m_ExpireState.insert(Expire(time(nullptr) + RETRY_SEC, aSerial));
        }

        void start()
        {
            m_Socket.async_receive(boost::asio::buffer(m_Buffer)
                                  , [this](const boost::system::error_code& aError, std::size_t aBytes) {
                                        if (!aError)
                                            this->cb(aBytes);
                                    });
        }

        void cb(size_t aSize)
        {
            assert(aSize == sizeof(Reply));
            const Reply* sReply = reinterpret_cast<const Reply*>(m_Buffer.data());

            if (sReply->flag != FLAG_STOP)
                m_Sender.hello();
            else
                m_Sender.stop();

            if (sReply->serial > 0)
            {
                std::cerr << "Server: recv ack for " << sReply->serial << std::endl;
                m_Sender.ack(sReply->serial);
                get<EXPIRE_BY_SERIAL>(m_ExpireState).erase(sReply->serial);
            } else {
                std::cerr << "Server: recv hello " << std::endl;
            }
            start();
        }

        void set_timer()
        {
            m_RetryTimer.expires_from_now(boost::posix_time::seconds(1));
            m_RetryTimer.async_wait([this](auto error){ if (!error) retry_proc(); });
        }

        void retry_proc()
        {
            bool sWasRetry = false;
            const time_t sDeadline = ::time(nullptr);
            for (const auto& x : get<EXPIRE_BY_TIME>(m_ExpireState))
                if (x.timeout < sDeadline)
                {
                    std::cerr << "Server: retry for " << x.serial << std::endl;
                    m_Sender.restore(x.serial);
                    sWasRetry = true;
                }
                else
                    break;

            if (!sWasRetry)
            {
                const auto sMin = m_Sender.minSerial();
                std::cerr << "Server: send heartbeat " << sMin << std::endl;
                const Header sHeader{sMin, 0, 0, 0, 0};
                m_Socket.send_to(asio::buffer(&sHeader, sizeof(sHeader)), m_Endpoint);
            }

            set_timer();
        }

    public:
        Server(const Config& aConfig, Threads::WorkQ& aWorkQ)
        : m_Endpoint(aConfig.peer())
        , m_Socket(aWorkQ.service(), aConfig.bind())
        , m_Sender(aWorkQ, this)
        , m_RetryTimer(aWorkQ.service())
        {
            std::cerr << "Server: bind to " <<  aConfig.bind() << std::endl;
            m_Buffer.resize(sizeof(Header));
            set_timer();
            start();
        }

        void push(const std::string& aBody) { m_Sender.push(aBody); }

        void wait()
        {
            std::cerr << "Server: wait" << std::endl;
            using namespace std::chrono_literals;
            while(!m_Sender.empty())
                std::this_thread::sleep_for(0.1s);
            std::cerr << "Server: stopped" << std::endl;
        }

        ~Server()
        {
            wait();
        }
    };
}
