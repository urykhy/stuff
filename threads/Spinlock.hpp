#pragma once

#include <atomic>
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

} // namespace Threads