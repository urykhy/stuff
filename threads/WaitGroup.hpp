#pragma once

#include <mutex>
#include <condition_variable>

namespace Threads {
    class WaitGroup {
        size_t m_Wait;
        std::mutex m_Mutex;
        std::condition_variable m_Cond;
        using Lock = std::unique_lock<std::mutex>;

    public:
        WaitGroup(size_t aInitial)
        : m_Wait(aInitial)
        { }

        void release() {
            Lock lk(m_Mutex);
            if (m_Wait > 0)
                m_Wait--;
            if (m_Wait == 0)
                m_Cond.notify_all();
        }

        void wait() {
            Lock lk(m_Mutex);
            if (m_Wait == 0)
                return;
            m_Cond.wait(lk, [this](){ return m_Wait == 0; });
        }

        ~WaitGroup() { wait(); }
    };
}