#pragma once

#include <mutex>
#include <future>

#include <event/Client.hpp>
#include <event/Framed.hpp>
#include <rpc/ReplyWaiter.hpp>

#include "Message.hpp"
#include "Protocol.hpp"
#include "Auth.hpp"

namespace tnt17
{
    template<class T>
    struct Client : public std::enable_shared_from_this<Client<T>>
    {
        using Notify = std::function<void(std::exception_ptr)>;
        using Result = std::vector<T>;
        using Handler = std::function<void(std::future<Result>&&)>;

    private:
        static constexpr unsigned CONNECT_TIMEOUT = 100;
        using tcp = boost::asio::ip::tcp;
        using Transport= Event::Framed::Client<Message>;

        std::shared_ptr<Event::Client> m_Client;
        std::shared_ptr<Transport> m_Transport;
        std::shared_ptr<Auth> m_Auth;

        boost::asio::io_service& m_Loop;
        std::shared_ptr<RPC::ReplyWaiter> m_Queue;
        std::atomic<uint64_t> m_Serial{1};
        const tcp::endpoint m_Addr;
        const int m_SpaceID;
        const Notify m_Notify;
        std::atomic_bool m_Connected{false};

        void xcall(Handler& aHandler, std::future<std::string>&& aReply)
        {
            std::promise<std::vector<T>> sPromise;

            try {
                const std::string sData = aReply.get();
                imemstream sStream(sData);
                const uint32_t sCount = MsgPack::read_array_size(sStream);
                std::vector<T> sResult{sCount};
                for (auto& x : sResult)
                    x.parse(sStream);
                sPromise.set_value(sResult);
            } catch (const std::exception& e) {
                sPromise.set_exception(std::make_exception_ptr(Event::ProtocolError(e.what())));
            }

            aHandler(sPromise.get_future());
        }

        void callback(std::future<std::string>&& aResult)
        {
            std::string sResultStr;
            try {
                sResultStr = aResult.get();
            } catch (...) {
                // must be network error
                notify(std::current_exception());
                return;
            }

            Header sHeader;
            Reply sReply;
            imemstream sStream(sResultStr);

            try {
                sHeader.parse(sStream);
                sReply.parse(sStream);
            } catch (const std::exception& e) {
                // protocol error, no need to close connection, just notify
                m_Notify(std::make_exception_ptr(Event::ProtocolError(e.what())));
                return;
            }

            std::promise<std::string> sPromise;
            if (sReply.ok) {
                const auto sRest = sStream.rest();
                std::string sRestStr(sRest.begin(), sRest.end());
                sPromise.set_value(sRestStr);
            } else {
                sPromise.set_exception(std::make_exception_ptr(Event::RemoteError(sReply.error)));
            }

            m_Queue->call(sHeader.sync, sPromise.get_future());
        }

        void notify(std::exception_ptr aPtr)
        {
            if (aPtr == nullptr)
                m_Connected = true;
            else
            {
                stop();
                if (m_Queue)
                    m_Queue->network_error(aPtr);
            }
            m_Notify(aPtr);
        }

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, int aSpaceID, Notify aNotify)
        : m_Loop(aLoop)
        , m_Queue(std::make_shared<RPC::ReplyWaiter>(aLoop))
        , m_Addr(aAddr)
        , m_SpaceID(aSpaceID)
        , m_Notify(aNotify)
        { }

        void start()
        {
            m_Client = std::make_shared<Event::Client>(m_Loop);
            m_Client->start(m_Addr, CONNECT_TIMEOUT, [this, p=this->shared_from_this()](std::future<tcp::socket&> aSocket)
            {
                try {
                    auto& sSocket = aSocket.get();
                    m_Auth = std::make_shared<Auth>(sSocket, [this, p, &sSocket](boost::system::error_code ec){
                        if (ec) {
                            notify(std::make_exception_ptr(Event::NetworkError(ec)));
                            return;
                        }
                        m_Transport = std::make_shared<Transport>(sSocket, [p](std::future<std::string>&& aResult){
                            p->callback(std::move(aResult));
                        });
                        m_Transport->start();
                        m_Queue->start();
                        notify(nullptr);   // connection established
                    });
                    m_Auth->start();
                } catch (...) {
                    notify(std::current_exception());
                }
            });
        }

        void stop()
        {
            m_Connected = false;
            if (m_Client)    { m_Client->stop(); m_Client.reset();}
            if (m_Queue)     { m_Queue->stop();  m_Queue.reset(); }
            if (m_Transport) { m_Transport.reset(); }
        }

        bool is_open() const { return m_Connected; }

        template<class K>
        void select(const IndexSpec& aIndex, const K& aKey, Handler aHandler, unsigned aTimeoutMs = 1000)
        {
            if (!is_open()) {
                std::promise<std::vector<T>> sPromise;
                sPromise.set_exception(std::make_exception_ptr(Event::NetworkError(boost::system::errc::make_error_code(boost::system::errc::not_connected))));
                aHandler(sPromise.get_future());
                return;
            }

            const uint64_t sSerial = m_Serial++;
            m_Queue->insert(sSerial, aTimeoutMs, [p=this->shared_from_this(), aHandler](std::future<std::string>&& aReply) mutable {
                p->xcall(aHandler, std::move(aReply));
            });

            MsgPack::binary sBuffer;
            MsgPack::omemstream sStream(sBuffer);
            formatHeader(sStream, CODE_SELECT, sSerial);
            formatSelectBody(sStream, m_SpaceID, aIndex);
            T::formatKey(sStream, aKey);
            m_Transport->call(sBuffer);
        }
    };
}