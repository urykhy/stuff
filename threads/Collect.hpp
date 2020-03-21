#pragma once

#include <boost/asio.hpp>

namespace Threads
{
    // group small messages to large block
    struct Collect
    {
        //namespace asio = boost::asio;
        using Handler = std::function<void(std::string&&)>;
        enum { MAX_BYTES = 64 * 1024 };
    private:
        mutable std::mutex m_Mutex;
        typedef std::unique_lock<std::mutex> Lock;

        std::string                   m_Buffer;
        boost::asio::deadline_timer   m_WriteTimer;
        Handler                       m_Next;

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
        Collect(boost::asio::io_service& aService, const Handler& aHandler)
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

        bool empty() const
        {
            Lock lk(m_Mutex);
            return m_Buffer.empty();
        }
    };
}
