#pragma once

#include <map>
#include <mutex>
#include <functional>

#include <boost/asio.hpp>

#include <time/Meter.hpp>

namespace RPC
{
    struct ReplyWaiter : public std::enable_shared_from_this<ReplyWaiter>
    {
        using Handler = std::function<void(std::future<std::string>&&)>;

    private:
        const unsigned TIMEOUT_MS=1;
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        std::map<uint64_t, Handler> m_Waiters;          // serial to handler
        std::multimap<uint64_t, uint64_t> m_Timeouts;   // timeout to serial
        boost::asio::deadline_timer m_Timer;
        std::atomic_bool m_Running{true};

        void setup_timer()
        {
            if (!m_Running)
                return;
            m_Timer.expires_from_now(boost::posix_time::millisec(TIMEOUT_MS));
            m_Timer.async_wait([p=this->shared_from_this()](auto error){ if (!error) p->timeout_func(); });
        }

        void timeout_func()
        {
            Lock sLock(m_Mutex);

            auto sNow = Time::get_time().to_ms();
            for (auto it = m_Timeouts.begin(); it != m_Timeouts.end() && it->first < sNow;)
            {
                auto sIt = m_Waiters.find(it->second);
                if (sIt != m_Waiters.end())
                {
                    m_Timer.get_io_service().post([sHandler = sIt->second]()
                    {
                        std::promise<std::string> sPromise;
                        sPromise.set_exception(std::make_exception_ptr(Event::RemoteError("timeout")));
                        sHandler(sPromise.get_future());
                    });
                    m_Waiters.erase(sIt);
                }
                it = m_Timeouts.erase(it);
            }
            sLock.unlock();

            setup_timer();
        }

        void on_network_error(std::exception_ptr aPtr)
        {
            Lock sLock(m_Mutex);
            for (auto& x : m_Waiters)
            {
                auto& sHandler = x.second;
                std::promise<std::string> sPromise;
                sPromise.set_exception(aPtr);
                sHandler(sPromise.get_future());
            }
            m_Timeouts.clear();
            m_Waiters.clear();
        }

    public:
        ReplyWaiter(boost::asio::io_service& aLoop)
        : m_Timer(aLoop)
        { }

        void start() { m_Running = true; setup_timer(); }
        void stop() { m_Running = false; }

        void insert(uint64_t aSerial, unsigned aTimeoutMs, Handler aHandler)
        {
            Lock lk(m_Mutex);
            m_Waiters[aSerial] = aHandler;
            m_Timeouts.insert(std::make_pair(Time::get_time().to_ms() + aTimeoutMs, aSerial));
        }

        bool call(uint64_t aSerial, std::future<std::string>&& aResult)
        {
            Handler sHandler;

            Lock sLock(m_Mutex);
            auto sIt = m_Waiters.find(aSerial);
            if (sIt == m_Waiters.end())
                return false;
            sHandler = sIt->second;
            m_Waiters.erase(sIt);
            sLock.unlock();

            sHandler(std::move(aResult));
            return true;
        }

        void network_error(std::exception_ptr aPtr) {
            m_Timer.get_io_service().post([p=this->shared_from_this(), aPtr](){
                p->on_network_error(aPtr);
            });
        }
    };
} // namespace RPC