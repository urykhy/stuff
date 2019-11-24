#pragma once

#include <map>
#include <mutex>
#include <functional>

#include <boost/asio.hpp>

#include <event/State.hpp>
#include <time/Meter.hpp>

namespace RPC
{
    // must be used with external timer / locking
    struct ReplyWaiter
    {
        using Handler = std::function<void(std::future<std::string>&&)>;

    private:
        std::map<uint64_t, Handler> m_Waiters;          // serial to handler
        std::multimap<uint64_t, uint64_t> m_Timeouts;   // timeout to serial
        std::exception_ptr m_Error;

        // got error, call handler
        void call(const Handler& aHandler, std::exception_ptr aPtr)
        {
            std::promise<std::string> sPromise;
            sPromise.set_exception(aPtr);
            aHandler(sPromise.get_future());
        }

        // mark all calls as failed
        void flush_int()
        {
            for (auto& x : m_Waiters)
                call(x.second, m_Error);
            m_Timeouts.clear();
            m_Waiters.clear();
        }
    public:

        bool empty() const { return m_Waiters.size(); }

        // insert call
        void insert(uint64_t aSerial, unsigned aTimeoutMs, Handler aHandler)
        {
            m_Waiters[aSerial] = aHandler;
            m_Timeouts.insert(std::make_pair(Time::get_time().to_ms() + aTimeoutMs, aSerial));
        }

        // got reply, call handler
        bool call(uint64_t aSerial, std::future<std::string>&& aResult)
        {
            Handler sHandler;

            auto sIt = m_Waiters.find(aSerial);
            if (sIt == m_Waiters.end())
                return false;
            sHandler = std::move(sIt->second);
            m_Waiters.erase(sIt);

            sHandler(std::move(aResult));
            return true;
        }

        // periodic check for timeouts
        void on_timer()
        {
            if (m_Error != nullptr)
            {
                flush_int();
                return;
            }

            auto sNow = Time::get_time().to_ms();
            for (auto it = m_Timeouts.begin(); it != m_Timeouts.end() && it->first < sNow;)
            {
                auto sIt = m_Waiters.find(it->second);
                if (sIt != m_Waiters.end())
                {
                    call(sIt->second, std::make_exception_ptr(Event::RemoteError("timeout")));
                    m_Waiters.erase(sIt);
                }
                it = m_Timeouts.erase(it);
            }
        }

        // mark all calls as failed
        void flush(std::exception_ptr aPtr)
        {
            m_Error = aPtr;
            flush_int();
        }
    };
} // namespace RPC