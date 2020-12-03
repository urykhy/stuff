#pragma once

#include <functional>
#include <map>
#include <mutex>

#include <boost/asio.hpp>

#include <time/Meter.hpp>

#include "Error.hpp"

namespace asio_tnt {

    using Promise = std::promise<std::string_view>;
    using Future  = std::future<std::string_view>;

    // must be used with external timer / locking
    struct ReplyWaiter
    {
        using Handler = std::function<void(Future&&)>;

    private:
        std::map<uint64_t, Handler>       m_Waiters;  // serial to handler
        std::multimap<uint64_t, uint64_t> m_Timeouts; // timeout to serial
        std::exception_ptr                m_Error;    // if not null - we in failed state, call all handlers with error

        // call handler with error
        void call(const Handler& aHandler, std::exception_ptr aPtr)
        {
            Promise sPromise;
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
        void insert(uint64_t aSerial, unsigned aTimeoutMs, Handler&& aHandler)
        {
            m_Waiters[aSerial] = std::move(aHandler);
            m_Timeouts.insert(std::make_pair(Time::get_time().to_ms() + aTimeoutMs, aSerial));
        }

        // call handler with aResult
        bool call(uint64_t aSerial, Future&& aResult)
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
            if (m_Error != nullptr) {
                flush_int();
                return;
            }

            auto sNow = Time::get_time().to_ms();
            for (auto it = m_Timeouts.begin(); it != m_Timeouts.end() && it->first < sNow;) {
                auto sIt = m_Waiters.find(it->second);
                if (sIt != m_Waiters.end()) {
                    call(sIt->second, std::make_exception_ptr(NetworkError(boost::system::errc::timed_out)));
                    m_Waiters.erase(sIt);
                }
                it = m_Timeouts.erase(it);
            }
        }

        // call all handlers with error
        void flush(std::exception_ptr aPtr)
        {
            m_Error = aPtr;
            flush_int();
        }

        void reset() { m_Error = nullptr; }
    };
} // namespace asio_tnt