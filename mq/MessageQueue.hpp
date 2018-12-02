#pragma once

#include <mutex>
#include <WorkQ.hpp>

namespace MQ
{
    namespace asio = boost::asio;

    // sender call network transport to write out query
    struct SenderTransport
    {
        virtual void push(size_t, const std::string&, size_t) = 0;
    };

    enum { MAX_BYTES = 65000 };

    namespace aux
    {
        using Handler = std::function<void(std::string&&)>;

        // group small messages to large block
        struct Group
        {
        private:
            mutable std::mutex m_Mutex;
            typedef std::unique_lock<std::mutex> Lock;


            std::string                   m_Buffer;
            Handler                       m_Next;
            boost::asio::deadline_timer   m_WriteTimer;

            void write_collected()
            {
                Lock lk(m_Mutex);
                if (!m_Buffer.empty())
                {
                    m_Next(std::move(m_Buffer));
                    m_Buffer.clear();
                }
                set_timer();
            }

            void set_timer()
            {
                m_WriteTimer.expires_from_now(boost::posix_time::seconds(1));
                m_WriteTimer.async_wait([this](auto error){ if (!error) write_collected(); });
            }

        public:
            Group(boost::asio::io_service& aService, const Handler& aHandler)
            : m_WriteTimer(aService)
            , m_Next(aHandler)
            {
                set_timer();
            }

            void push(const std::string& aBody)
            {
                Lock lk(m_Mutex);
                if (m_Buffer.size() + aBody.size() > MAX_BYTES)
                {
                    m_Next(std::move(m_Buffer));
                    m_Buffer.clear();
                }
                m_Buffer.append(aBody);
            }
        };

        // user can call size() to monitor queue size
        //
        // if Transport->push can handle socker error, or wait in queue too long
        // restore will cause double-send.
        // client should handle this
        //
        // TODO: add queue size limit ?
        //       report queue size in bytes
        class Sender
        {
            mutable std::mutex m_Mutex;
            typedef std::unique_lock<std::mutex> Lock;

            Threads::WorkQ&  m_WorkQ;
            Group            m_Group;
            SenderTransport* m_Transport;
            size_t           m_Serial = 0;

            using Queue = std::map<size_t, std::string>;
            Queue m_Emit;

            void writeOut(std::string&& aBody)
            {
                Lock lk(m_Mutex);
                const auto sSerial = ++m_Serial;
                m_Emit[sSerial] = aBody;
                const size_t sMinSerial = m_Emit.begin()->first;
                m_WorkQ.insert([sSerial, aBody, sMinSerial, this](){ m_Transport->push(sSerial, aBody, sMinSerial); });
            }

        public:
            Sender(Threads::WorkQ& aWorkQ, SenderTransport* aTransport)
            : m_WorkQ(aWorkQ)
            , m_Group(aWorkQ.service(), [this](std::string&& aBody){ writeOut(std::move(aBody)); })
            , m_Transport(aTransport)
            {
                // silly start with `random` serial, to avoid repeating used values
                m_Serial = time(nullptr) << 32;
            }

            void push(const std::string& aBody)
            {
                m_Group.push(aBody);
            }

            size_t size() const
            {
                Lock lk(m_Mutex);
                return m_Emit.size();
            }

            void ack(size_t x)
            {
                Lock lk(m_Mutex);
                m_Emit.erase(x);
            }

            size_t minSerial() const
            {
                Lock lk(m_Mutex);
                if (m_Emit.empty())
                    return 0;
                return m_Emit.begin()->first;
            }

            void restore(size_t aId)
            {
                Lock lk(m_Mutex);
                if (m_Emit.empty())
                    return;
                const size_t sMinSerial = m_Emit.begin()->first;

                const auto sIt = m_Emit.find(aId);
                if (sIt == m_Emit.end())
                    return;

                m_WorkQ.insert([x=*sIt, sMinSerial, this](){ m_Transport->push(x.first, x.second, sMinSerial); });
            }
        };

        // network transport must call push, and then send ack to server
        // only one peer supported
        class Receiver
        {
            mutable std::mutex m_Mutex;
            typedef std::unique_lock<std::mutex> Lock;
            std::set<uint64_t> m_History;
            Handler            m_Handler;
            uint64_t           m_StartId = 0;

        public:
            Receiver(Handler aHandler) : m_Handler(aHandler) {}

            size_t size() const { return m_History.size(); }

            // clear history
            void clear(uint64_t aSerial)
            {
                while (!m_History.empty() && *m_History.begin() < aSerial)
                    m_History.erase(m_History.begin());
                m_StartId = aSerial;
            }

            // on query
            void push(uint64_t aSerial, std::string&& aBody)
            {
                Lock lk(m_Mutex);
                if (aSerial >= m_StartId && m_History.count(aSerial) == 0)
                {
                    lk.unlock();    // call handler without mutex held
                    m_Handler(std::move(aBody));
                    lk.lock();
                    m_History.insert(aSerial);
                }
            }
        };

    } // namespace aux
}
