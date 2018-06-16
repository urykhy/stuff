#pragma once
#include <SafeQueue.hpp>
#include <Periodic.hpp>

namespace Threads
{
    template<class T>
    class SafeQueueWorker
    {
        std::atomic<bool> exit_flag{false};
        std::mutex mutex;
        std::condition_variable cond;
        typedef std::unique_lock<std::mutex> Lock;

        std::list<T> list;
        std::function<bool(T& t)> handler;

        const time_t retry_delay = 10;

        void thread_proc()
        {
            time_t dead_until = 0;

            while(!exit_flag)
            {
                if (dead_until > time(nullptr)) {
                    sleep(1);
                    continue;
                }

                Lock lk(mutex);
                if (list.empty()) {
                    cond.wait_for(lk, std::chrono::milliseconds(500));
                }
                if (exit_flag || list.empty())
                    continue;
                T& current = list.front();
                lk.unlock();
                if (handler(current))
                {
                    lk.lock();
                    list.pop_front();
                } else {
                    dead_until = time(nullptr) + retry_delay;
                }
            }
        }

    public:
        template<class F>
        SafeQueueWorker(F f) : handler(f) {}

        void insert(T&& t)
        {
            Lock lk(mutex);
            list.push_back(std::move(t));
            cond.notify_one();
        }

        void stop() { exit_flag = true; }

        void start(Group& tg)
        {
            tg.start([this](){
                thread_proc();
            });
            tg_at_stop([this](){
                stop();
            });
        }
    };
}
