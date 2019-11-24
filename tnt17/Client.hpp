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
        using tcp = boost::asio::ip::tcp;
        using Transport= Event::Framed::Client<Message>;
    private:

        std::shared_ptr<Event::Client> m_Client;
        std::shared_ptr<Transport> m_Transport;
        std::shared_ptr<Auth> m_Auth;

        boost::asio::io_service& m_Loop;
        boost::asio::deadline_timer m_Timer;
        RPC::ReplyWaiter m_Queue;

        using Lock = std::unique_ptr<std::mutex>;
        std::mutex m_Mutex;

        std::atomic<uint64_t> m_Serial{1};
        const tcp::endpoint m_Addr;
        const int m_SpaceID;
        const Notify m_Notify;

        // stage1. callback to decode server response and find proper callback to call
        void callback(std::future<std::string>&& aResult)
        {
            std::string sResultStr;
            try {
                sResultStr = aResult.get();
            } catch (...) {
                notify(std::current_exception()); // must be network error
                return;
            }

            Header sHeader;
            Reply sReply;
            imemstream sStream(sResultStr);

            try {
                sHeader.parse(sStream);
                sReply.parse(sStream);
            } catch (const std::exception& e) {
                // if we can't parse protocol - notify as fatal error
                notify(std::make_exception_ptr(Event::ProtocolError(e.what())));
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
        }

        // stage2. callback from ReplyWaiter. decode response and jump to user-provided callback
        void xcall(const Handler& aHandler, std::future<std::string>&& aReply)
        {
            std::promise<std::vector<T>> sPromise;
            std::string sData;

            try {
                sData = aReply.get();
            }
            catch (...) {
                sPromise.set_exception(std::current_exception());
                aHandler(sPromise.get_future());
                return;
            };

            try {
                imemstream sStream(sData);
                const uint32_t sCount = MsgPack::read_array_size(sStream);
                std::vector<T> sResult{sCount};
                for (auto& x : sResult)
                    x.parse(sStream);
                sPromise.set_value(sResult);
            } catch (const std::exception& e) {   // fail to parse response
                sPromise.set_exception(std::make_exception_ptr(Event::ProtocolError(e.what())));
            }
            aHandler(sPromise.get_future());
        }

        // called on state changes: connection established or network error
        void notify(std::exception_ptr aPtr)
        {
            if (aPtr != nullptr)
            {
                m_Queue.flush(aPtr); // flush call queue with error
                stop();              // close sockets and so on on hard error
            }
            m_Notify(aPtr);
        }

        // timer to handle timeouts
        void set_timer()
        {
            const unsigned TIMEOUT_MS = 1; // check timeouts every 1ms
            if (!m_Client->is_running() and m_Queue.empty())
                return;
            m_Timer.expires_from_now(boost::posix_time::millisec(TIMEOUT_MS));
            m_Timer.async_wait(m_Client->wrap([p=this->shared_from_this()](auto error){ if (!error) p->timeout_func(); }));
        }

        void timeout_func()
        {
            m_Queue.on_timer(); // report timeout on slow calls
            set_timer();
        }

    public:
        Client(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, int aSpaceID, Notify aNotify)
        : m_Loop(aLoop)
        , m_Timer(aLoop)
        , m_Addr(aAddr)
        , m_SpaceID(aSpaceID)
        , m_Notify(aNotify)
        { }

        void start()
        {
            constexpr unsigned CONNECT_TIMEOUT = 100;
            m_Client = std::make_shared<Event::Client>(m_Loop, m_Addr, CONNECT_TIMEOUT, [this, p=this->shared_from_this()](std::future<Event::Client::Ptr> aClient)
            {
                try {
                    auto sClient = aClient.get();
                    m_Auth = std::make_shared<Auth>(sClient, [this, p, sClient](boost::system::error_code ec){
                        m_Auth.reset();
                        if (ec) {
                            notify(std::make_exception_ptr(Event::NetworkError(ec)));
                            return;
                        }
                        m_Transport = std::make_shared<Transport>(sClient, [p](std::future<std::string>&& aResult){
                            p->callback(std::move(aResult));
                        });
                        m_Transport->start();
                        notify(nullptr);   // connection established
                        set_timer();       // start timer to handle call timeouts
                    });
                    m_Auth->start();
                } catch (...) {
                    notify(std::current_exception());
                }
            });
            m_Client->start();
        }

        void stop()
        {
            if (m_Client) {
                m_Client->stop();
            }
        }

        bool is_connected() const { return m_Client && m_Client->is_connected(); }

        template<class K>
        bool select(const IndexSpec& aIndex, const K& aKey, Handler aHandler, unsigned aTimeoutMs = 1000)
        {
            const uint64_t sSerial = m_Serial++;
            MsgPack::binary sBuffer;
            MsgPack::omemstream sStream(sBuffer);
            formatHeader(sStream, CODE_SELECT, sSerial);
            formatSelectBody(sStream, m_SpaceID, aIndex);
            T::formatKey(sStream, aKey);

            if (!m_Client || !m_Client->is_connected())
                return false;

            m_Client->post([this, p=this->shared_from_this(), aHandler, sSerial, sBuffer, aTimeoutMs] () mutable
            {
                m_Queue.insert(sSerial, aTimeoutMs, [p, aHandler](std::future<std::string>&& aReply) {
                    p->xcall(aHandler, std::move(aReply));
                });
                m_Transport->call(sBuffer);
            });
        }
    };
}