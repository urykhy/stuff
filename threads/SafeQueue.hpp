#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <set>

#include "Group.hpp"

namespace Threads {
    template <class T>
    struct ListWrapper
    {
        std::list<T> m_List;

        void   push(T&& t) { m_List.push_back(std::move(t)); }
        void   push(const T& t) { m_List.push_back(t); }
        bool   empty() const { return m_List.empty(); }
        T&     top() { return m_List.front(); }
        void   pop() { m_List.pop_front(); }
        size_t size() const { return m_List.size(); }
    };

    // NOTE: size is not limited
    // fast exit only: do not wait for all tasks to complete
    template <class Node, class Q = ListWrapper<Node>>
    class SafeQueue
    {
        mutable std::mutex                   m_Mutex;
        typedef std::unique_lock<std::mutex> Lock;
        Q                                    m_List;
        std::condition_variable              m_Cond;
        bool                                 m_Stop    = false;
        uint64_t                             m_Counter = 0;

        void wakeup_one() { m_Cond.notify_one(); }
        void wakeup_all() { m_Cond.notify_all(); }
        void wait_for(Lock& aLock) { m_Cond.wait_for(aLock, std::chrono::milliseconds(500)); }

    public:
        SafeQueue() {}

        void stop()
        {
            Lock lk(m_Mutex);
            m_Stop = true;
            wakeup_all();
        }

        void insert(const Node& aItem)
        {
            Lock lk(m_Mutex);
            m_List.push(aItem);
            m_Counter++;
            wakeup_one();
        }

        void insert(Node&& aItem)
        {
            Lock lk(m_Mutex);
            m_List.push(std::move(aItem));
            m_Counter++;
            wakeup_one();
        }

        bool exiting() const
        {
            Lock lk(m_Mutex);
            return m_Stop;
        }

        bool idle() const
        {
            Lock lk(m_Mutex);
            return m_List.empty();
        }

        uint64_t count() const
        {
            Lock lk(m_Mutex);
            return m_Counter;
        }

        uint64_t pending() const
        {
            Lock lk(m_Mutex);
            return m_List.size();
        }

        std::optional<Node> try_get()
        {
            std::optional<Node> sResult;
            Lock                lk(m_Mutex);
            if (m_List.empty())
                return sResult;
            sResult = std::move(m_List.top());
            m_List.pop();
            return sResult;
        }

        bool wait(Node& aItem, std::function<bool(const Node&)> aTest = {})
        {
            Lock lk(m_Mutex);

            if (m_List.empty())
                wait_for(lk);

            if (m_List.empty())
                return false;

            if (aTest and !aTest(m_List.top())) { // have some data, but we can't consume it yet. so pause a bit
                wait_for(lk);
                return false;
            }

            aItem = std::move(m_List.top());
            m_List.pop();
            if (!m_List.empty())
                wakeup_one(); // cond var can miss wakeups, so we try to wakeup next thread from here
            return true;
        }
#ifdef BOOST_TEST_MODULE
        auto& debug()
        {
            return m_List;
        }
#endif
    };

    // just a queue and thread to process tasks
    // default - fast exit (no wait until tasks processed)
    template <class T, class Q = ListWrapper<T>>
    struct SafeQueueThread
    {
        struct Params
        {
            bool   retry  = false; // retry on exception
            float  delay  = 1;     // sleep before next retry in seconds
            time_t linger = 0;     // delay exit if have pending tasks

            std::function<bool(const T&)> check = {}; // condition to pick task from queue
            std::function<void()>         idle  = {}; // called if no tasks to process
        };

    private:
        const std::function<void(T& t)> m_Handler;
        const Params                    m_Params;

        SafeQueue<T, Q>       m_Queue;
        std::atomic<uint64_t> m_Done{0};

        mutable std::mutex              m_Mutex;
        std::unique_ptr<Time::Deadline> m_Deadline;

        bool exiting() const
        {
            if (!m_Queue.exiting())
                return false;
            std::unique_lock lk(m_Mutex);
            return !m_Deadline or (idle() or m_Deadline->expired());
        }

        void handle(T& aItem)
        {
            while (!exiting()) {
                try {
                    m_Handler(aItem);
                    return;
                } catch (...) {
                    if (!m_Params.retry)
                        return;
                    Threads::sleep(m_Params.delay);
                }
            }
        }

    public:
        template <class F>
        SafeQueueThread(F aHandler, const Params& aParams = Params())
        : m_Handler(aHandler)
        , m_Params(aParams)
        {
        }

        void start(Group& aGroup, unsigned aCount = 1)
        {
            aGroup.start(
                [this]() {
                    while (!exiting()) {
                        T sItem;
                        if (m_Queue.wait(sItem, m_Params.check)) {
                            handle(sItem);
                            m_Done++;
                        } else {
                            if (m_Params.idle)
                                m_Params.idle();
                        }
                    }
                },
                aCount);
            aGroup.at_stop([this]() {
                if (m_Params.linger > 0) {
                    std::unique_lock lk(m_Mutex);
                    m_Deadline = std::make_unique<Time::Deadline>(m_Params.linger);
                }
                m_Queue.stop();
            });
        }

        void   insert(const T& aItem) { m_Queue.insert(aItem); }
        void   insert(T&& aItem) { m_Queue.insert(std::move(aItem)); }
        bool   idle() const { return m_Done == m_Queue.count(); }
        size_t size() const { return m_Queue.count() - m_Done; }
        void   wait(time_t aMax)
        {
            const double aStep = aMax / (double)100;
            for (int i = 0; i < 100; i++) {
                if (idle())
                    break;
                sleep(aStep);
            }
        }
#ifdef BOOST_TEST_MODULE
        auto& debug()
        {
            return m_Queue.debug();
        }
#endif
    };

    struct QueueExecutor : public SafeQueueThread<std::function<void()>>
    {
        QueueExecutor()
        : SafeQueueThread<std::function<void()>>([](auto& aTask) { aTask(); })
        {
        }
    };
} // namespace Threads