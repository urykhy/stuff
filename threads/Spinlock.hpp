#pragma once

#include <pthread.h>

#include <atomic>
#include <memory>
#include <thread>

namespace Threads {

    // via: https://probablydance.com/2019/12/30/measuring-mutexes-spinlocks-and-how-bad-the-linux-scheduler-really-is/
    struct Spinlock
    {
        void lock()
        {
            while (!try_lock())
                std::this_thread::yield();
        }
        bool try_lock()
        {
            return !locked.load(std::memory_order_relaxed) && !locked.exchange(true, std::memory_order_acquire);
        }
        void unlock()
        {
            locked.store(false, std::memory_order_release);
        }

    private:
        std::atomic<bool> locked{false};
    };

    class Adaptive
    {
        pthread_mutex_t m_Mutex;

    public:
        Adaptive()
        {
            pthread_mutexattr_t sAttr;
            pthread_mutexattr_init(&sAttr);
            pthread_mutexattr_settype(&sAttr, PTHREAD_MUTEX_ADAPTIVE_NP);
            pthread_mutex_init(&m_Mutex, &sAttr);
        }
        void lock()
        {
            pthread_mutex_lock(&m_Mutex);
        }
        void unlock()
        {
            pthread_mutex_unlock(&m_Mutex);
        }
    };

    template <class T>
    class AtomicSharedPtr
    {
        using Ptr = std::shared_ptr<T>;
        std::atomic<Ptr> m_Ptr;

    public:
        AtomicSharedPtr(Ptr aPtr = std::make_shared<T>())
        : m_Ptr(aPtr)
        {
        }

        Ptr Read() const
        {
            return m_Ptr.load();
        }

        template <class H>
        void Update(H&& aHandler)
        {
            Ptr sCurrent;
            Ptr sUpdate;
            do {
                sCurrent = m_Ptr.load();
                sUpdate  = std::make_shared<T>(*sCurrent);
                aHandler(sUpdate);
            } while (!m_Ptr.compare_exchange_weak(sCurrent, sUpdate));
        }
    };
} // namespace Threads