#pragma once

#include <mutex>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <time/Meter.hpp>

namespace container
{
    namespace {
        namespace mi = boost::multi_index;
    }

    template<class V>
    struct RequestQueue
    {
        using Handler = std::function<void(V&)>;

    private:
        struct Rec
        {
            uint64_t serial;
            uint64_t deadline;
            mutable V value; // pass not const value to handler
        };
        struct by_serial {};
        struct by_deadline {};
        using Store = boost::multi_index_container<
            Rec,
            mi::indexed_by<
                mi::ordered_unique    <mi::tag<by_serial>,   mi::member<Rec, uint64_t, &Rec::serial>>,
                mi::ordered_non_unique<mi::tag<by_deadline>, mi::member<Rec, uint64_t, &Rec::deadline>>
            >
        >;
        using    Lock = std::unique_lock<std::mutex>;
        mutable  std::mutex m_Mutex;

        Store    m_Store;
        uint64_t m_Serial{0};
        Handler  m_Handler;  // called under mutex

    public:
        RequestQueue(Handler aHandler) : m_Handler(aHandler) {}

        using Value = std::optional<V>;
        Value get()
        {
            Lock lk(m_Mutex);
            auto sNow = Time::get_time().to_ms();
            auto& sStore = boost::multi_index::get<by_serial>(m_Store);

            while (!sStore.empty())
            {
                auto sIt = sStore.begin();
                if (sIt->deadline <= sNow)
                {   // notify if timeout
                    m_Handler(sIt->value);
                    sStore.erase(sIt);
                    continue;
                }
                Value sResult = std::move(sIt->value);
                sStore.erase(sIt);
                return sResult;
            }

            return std::nullopt;
        }

        uint64_t insert(V&& aValue, uint64_t aTimeoutMs)
        {
            Lock lk(m_Mutex);
            uint64_t sID = m_Serial++;
            m_Store.insert(Rec{sID, Time::get_time().to_ms() + aTimeoutMs, std::move(aValue)});
            return sID;
        }

        // get time in ms until next expiration or aDefault
        uint64_t eta(uint64_t aDefault) const
        {
            Lock lk(m_Mutex);
            auto sNow = Time::get_time().to_ms();
            auto& sStore = boost::multi_index::get<by_deadline>(m_Store);
            if (sStore.empty())
                return aDefault;
            auto sIt = sStore.begin();
            uint64_t sTimeout = sIt->deadline > sNow ? sIt->deadline - sNow : 0;
            return std::min(aDefault, sTimeout);
        }

        void on_timer()
        {
            std::list<V> sList;

            Lock lk(m_Mutex);
            auto sNow = Time::get_time().to_ms();
            auto& sStore = boost::multi_index::get<by_deadline>(m_Store);

            while (!sStore.empty())
            {
                auto sIt = sStore.begin();
                if (sIt->deadline <= sNow)
                {   // notify if timeout
                    sList.push_back(std::move(sIt->value));
                    sStore.erase(sIt);
                    continue;
                }
                break;
            }
            lk.unlock();

            // call handler's without mutex
            for (auto& x : sList)
                m_Handler(x);
        }

        bool empty()  const
        {
            Lock lk(m_Mutex);
            return m_Store.empty();
        }

        size_t size() const
        {
            Lock lk(m_Mutex);
            return m_Store.size();
        }
    };
}