#pragma once

#include <mutex>
#include <future>

#include <event/Client.hpp>
#include <event/Framed.hpp>
#include <rpc/ReplyWaiter.hpp>

#include "Message.hpp"
#include "Protocol.hpp"
#include "Auth.hpp"

//#include <parser/Hex.hpp>

namespace tnt17
{
    template<class T>
    struct Client
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
        RPC::ReplyWaiter m_Queue;
        std::atomic<uint64_t> m_Serial{1};
        const tcp::endpoint m_Addr;
        const int m_SpaceID;
        const Notify m_Notify;

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
                // FIXME: call m_Queue.error to mark all requests as failed
                stop();
                m_Notify(std::current_exception());
                return;
            }

            Header sHeader;
            Reply sReply;
            imemstream sStream(sResultStr);

            try {
                sHeader.parse(sStream);
                sReply.parse(sStream);
            } catch (const std::exception& e) {
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

            m_Queue.call(sHeader.sync, sPromise.get_future());
        }

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, int aSpaceID, Notify aNotify)
        : m_Loop(aLoop)
        , m_Queue(aLoop)
        , m_Addr(aAddr)
        , m_SpaceID(aSpaceID)
        , m_Notify(aNotify)
        { }

        void start()
        {
            m_Client = std::make_shared<Event::Client>(m_Loop);
            m_Client->start(m_Addr, CONNECT_TIMEOUT, [this](std::future<tcp::socket&> aSocket)
            {
                try {
                    auto& sSocket = aSocket.get();
                    m_Auth = std::make_shared<Auth>(sSocket, [this, &sSocket](boost::system::error_code ec){
                        if (ec) {
                            stop();
                            m_Notify(std::make_exception_ptr(Event::NetworkError(ec)));
                            return;
                        }
                        m_Transport = std::make_shared<Transport>(sSocket, [this, &sSocket](std::future<std::string>&& aResult){
                            callback(std::move(aResult));
                        });
                        m_Transport->start();
                        m_Notify(nullptr);   // connected ok
                    });
                    m_Auth->start();
                } catch (...) {
                    stop();
                    m_Notify(std::current_exception());
                }
            });
        }

        void stop() { m_Client->stop(); }
        bool is_open() const { return m_Client->is_open(); }

        template<class K>
        void select(const int aIndex, const K& aKey, Handler aHandler, unsigned aTimeoutMs = 100)
        {
            const uint64_t sSerial = m_Serial++;
            m_Queue.insert(sSerial, aTimeoutMs, [this, aHandler](std::future<std::string>&& aReply) mutable {
                xcall(aHandler, std::move(aReply));
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