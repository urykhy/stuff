#pragma once

#include <mutex>
#include <WorkQ.hpp>

namespace MQ
{
    namespace asio = boost::asio;

    // sender call network transport to write out query
    struct SenderTransport
    {
        virtual void push(size_t, const std::string&) = 0;
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
                    m_Next(std::move(m_Buffer));
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
                    m_Next(std::move(m_Buffer));
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
                m_WorkQ.insert([sSerial, aBody, this](){ m_Transport->push(sSerial, aBody); });
                m_Emit[sSerial] = std::move(aBody);
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

            // FIXME: add rate limit here ?
            void restore(size_t aId = 0)
            {
                Lock lk(m_Mutex);
                for (const auto& x : m_Emit)
                    if (aId == 0 or x.first == aId)
                        m_WorkQ.insert([x, this](){ m_Transport->push(x.first, x.second); });
            }
        };

        struct TaskSerial
        {
            uint64_t serial = 0;
            uint32_t ip = 0;
            uint16_t port = 0;

            bool operator<(const TaskSerial& aOther) const
            {
                return std::tie(serial, ip, port) < std::tie(aOther.serial, aOther.ip, aOther.port);
            }
        };

        // network transport must call push, and then send ack to server
        // FIXME: add function to save/restore state ?
        class Receiver
        {
            mutable std::mutex m_Mutex;
            typedef std::unique_lock<std::mutex> Lock;
            const unsigned   HISTORY_SIZE = 1024;
            std::set<TaskSerial> m_History;
            Handler          m_Handler;

        public:
            Receiver(Handler aHandler) : m_Handler(aHandler) {}

            size_t size() const { return m_History.size(); }

            // on query
            void push(const TaskSerial& aSerial, std::string&& aBody)
            {
                Lock lk(m_Mutex);
                if (m_History.count(aSerial) == 0)
                {
                    lk.unlock();    // call handler without mutex held
                    m_Handler(std::move(aBody));
                    lk.lock();
                    // FIXME: if multiple senders are used - multi_index + expiration time should be used
                    if (m_History.size() >= HISTORY_SIZE)
                        m_History.erase(m_History.begin());
                    m_History.insert(aSerial);
                }
            }
        };

    } // namespace aux
}
