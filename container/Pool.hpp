#pragma once

#include <functional>
#include <list>
#include <memory>

namespace Container
{
    template<class T>
    struct Pool
    {
        using Ptr = std::shared_ptr<T>;
        using Generate = std::function<Ptr()>;
        using Check = std::function<bool(Ptr)>;
    private:

        using Data = std::pair<time_t, Ptr>;
        std::list<Data> m_List;
        Generate m_Gen;
        Check m_Check;
        const size_t m_IdleCount;
        const time_t m_Timeout;

        bool is_timedout(time_t x) const { return x + m_Timeout < time(nullptr); }
    public:

        Pool(Generate g, Check c, size_t aIdleCount = 1, time_t aTimeout = 10)
        : m_Gen(g), m_Check(c), m_IdleCount(aIdleCount), m_Timeout(aTimeout)
        {}

        Ptr get()
        {
            while (!m_List.empty())
            {
                auto   i  = m_List.rbegin();    // pick element with max timestamp
                time_t ts = i->first;
                Ptr    x  = i->second;
                m_List.erase((++i).base());
                if (is_timedout(ts))
                    continue;
                if (m_Check(x))
                    return x;
            }

            return m_Gen();
        }

        void release(Ptr& t)
        {
            m_List.push_back(std::make_pair(time(nullptr), std::move(t)));
            if (m_List.size() > m_IdleCount)
                m_List.erase(m_List.begin());
        }

        // call to close timed out connections
        void cleanup()
        {
            while (is_timedout(m_List.begin()->first))
                m_List.erase(m_List.begin());
        }

        size_t size() const { return m_List.size(); }
    };
}