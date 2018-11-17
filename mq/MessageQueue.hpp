#pragma once

#include <mutex>
#include <WorkQ.hpp>

namespace MQ
{
    namespace asio = boost::asio;

    // group small messages to large block
    struct Group
    {
        using Handler = std::function<void(std::string&&)>;
    private:
        mutable std::mutex m_Mutex;
        typedef std::unique_lock<std::mutex> Lock;

        enum { MAX_BYTES = 65000 };

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

        void push(const std::string& aData)
        {
            Lock lk(m_Mutex);
            if (m_Buffer.size() + aData.size() > MAX_BYTES)
                m_Next(std::move(m_Buffer));
            m_Buffer.append(aData);
        }
    };

    struct SenderRecv
    {
        virtual void ack(size_t) = 0; // got ack from network
        virtual void restore()   = 0; // connection restored
    };

    struct SenderTransport
    {
        virtual void push(size_t, const std::string&) = 0;
    };

    // user can call size() to monitor queue size
    //
    // if Transport->push can handle socker error, or wait in queue too long
    // restore will cause double-send.
    // client should handle this
    //
    // TODO: add queue size limit ?
    //       report queue size in bytes
    class Sender : public SenderRecv
    {
        mutable std::mutex m_Mutex;
        typedef std::unique_lock<std::mutex> Lock;

        Threads::WorkQ&  m_WorkQ;
        Group            m_Group;
        SenderTransport* m_Transport;
        size_t           m_Serial = 0;

        using Queue = std::map<size_t, std::string>;
        Queue m_Emit;

        void writeOut(std::string&& aData)
        {
            Lock lk(m_Mutex);
            const auto sSerial = ++m_Serial;
            m_WorkQ.insert([sSerial, aData, this](){ m_Transport->push(sSerial, aData); });
            m_Emit[sSerial] = std::move(aData);
        }

        void ack(size_t x) override
        {
            Lock lk(m_Mutex);
            m_Emit.erase(x);
        }

        // FIXME: add rate limit here
        void restore() override
        {
            Lock lk(m_Mutex);
            for (const auto& x : m_Emit)
                m_WorkQ.insert([x, this](){ m_Transport->push(x.first, x.second); });
        }

    public:
        Sender(Threads::WorkQ& aWorkQ, SenderTransport* aTransport)
        : m_WorkQ(aWorkQ)
        , m_Group(aWorkQ.service(), [this](std::string&& aData){ writeOut(std::move(aData)); })
        , m_Transport(aTransport)
        { }

        void push(const std::string& aData)
        {
            m_Group.push(aData);
        }

        size_t size() const
        {
            Lock lk(m_Mutex);
            return m_Emit.size();
        }
    };

    // network transport must call push, and then send ack to server
    // FIXME: add function to save/restore state ?
    struct Receiver
    {
        using Handler = std::function<void(std::string&&)>;
    private:
        mutable std::mutex m_Mutex;
        typedef std::unique_lock<std::mutex> Lock;
        const unsigned   HISTORY_SIZE = 1024;
        std::set<size_t> m_History;
        Handler          m_Handler;

    public:
        Receiver(Handler aHandler) : m_Handler(aHandler) {}

        size_t size() const { return m_History.size(); }

        // on query
        void push(size_t aSerial, std::string&& aData)
        {
            Lock lk(m_Mutex);
            if (m_History.count(aSerial) == 0)
            {
                lk.unlock();    // call handler without mutex held
                m_Handler(std::move(aData));
                lk.lock();
                if (m_History.size() >= HISTORY_SIZE)
                    m_History.erase(m_History.begin());
                m_History.insert(aSerial);
            }
        }
    };

}
