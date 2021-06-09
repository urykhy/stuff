#pragma once
#include <cmath>
#include <functional>
#include <list>
#include <thread>

#include <time/Meter.hpp>

namespace Threads {
    struct Group
    {
        typedef std::function<void()> Handler;

    public:
        void start(Handler aHandler, size_t aCount = 1)
        {
            for (size_t i = 0; i < aCount; i++)
                m_Threads.push_back(std::thread(aHandler));
        }
        template <class F>
        void at_stop(F aStop) { m_Stoppers.push_back(aStop); }

        void wait()
        {
            for (auto& x : m_Stoppers)
                x();
            m_Stoppers.clear();
            for (auto& x : m_Threads)
                x.join();
            m_Threads.clear();
        }

        ~Group() throw() { wait(); }

    private:
        std::list<std::thread>               m_Threads;
        std::list<std::function<void(void)>> m_Stoppers;
    };

    inline void sleep(double aTime)
    {
        const Time::time_spec sTime(aTime);
        nanosleep(&sTime, nullptr);
    }
} // namespace Threads
