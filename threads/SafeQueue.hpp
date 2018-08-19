#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <set>
#include <queue>
#include <Group.hpp>

namespace Threads
{

    template<class T>
    struct ListWrapper
    {
        std::list<T> m_List;

        void push(const T& t) { m_List.push_back(t); }
        bool empty() const    { return m_List.empty(); }
        T&   top()            { return m_List.front(); }
        void pop()            { m_List.pop_front(); }
    };

    template<class Node, class Q = ListWrapper<Node>>
    class SafeQueue
    {
        std::atomic<bool> m_Stop{false};
        mutable std::mutex m_Mutex;
        typedef std::unique_lock<std::mutex> Lock;
        Q m_List;
        std::condition_variable m_Cond;
        unsigned m_WaitCount{0};

        void wakeup_one()
        {
            if (m_WaitCount > 0)
                m_Cond.notify_one();
        }

        void wakeup_all()
        {
            if (m_WaitCount > 0)
                m_Cond.notify_all();
        }

        void wait_for(Lock& aLock)
        {
            m_WaitCount++;
            m_Cond.wait_for(aLock, std::chrono::milliseconds(500));
            m_WaitCount--;
        }

    public:

        void stop()
        {
            m_Stop = true;
            Lock lk(m_Mutex);
            wakeup_all();
        }

        void insert(const Node& aItem)
        {
            Lock lk(m_Mutex);
            m_List.push(aItem);
            wakeup_one();
        }

        bool exiting() const
        {
            return m_Stop;
        }

        bool idle(unsigned aExpected) const
        {
            Lock lk(m_Mutex);
            assert (m_WaitCount <= aExpected);
            return m_List.empty() && m_WaitCount == aExpected;
        }

        bool wait(Node& aItem, std::function<bool(Node&)> aTest = [](Node&) -> bool { return true; })
        {
            while (!m_Stop)
            {
                Lock lk(m_Mutex);
                if (m_List.empty())
                    wait_for(lk);
                if (!m_Stop and !m_List.empty())
                {
                    auto sItem = m_List.top();
                    if (aTest(sItem))
                    {
                        m_List.pop();
                        if (!m_List.empty())
                            wakeup_one();   // cond var can miss wakeups, so we try to wakeup next thread from here
                        lk.unlock();
                        aItem = std::move(sItem);
                        return true;
                    } else {
                        wait_for(lk);
                    }
                }
            }
            return false;
        }
    };

    template<class T, class Q = ListWrapper<T>>
    class SafeQueueThread
    {
        const std::function<void(T& t)> m_Handler;
        SafeQueue<T, Q> m_Queue;
    public:
        template<class F> SafeQueueThread(F aHandler)
        : m_Handler(aHandler)  { }

        void start(Group& aGroup, std::function<bool(T&)> aCheck = [](T&) -> bool { return true; }, unsigned count = 1)
        {
            aGroup.start([this, aCheck]() {
                while (!m_Queue.exiting())
                {
                    T sItem;
                    if (m_Queue.wait(sItem, aCheck))
                        m_Handler(sItem);
                }
            }, count);
            aGroup.at_stop([this](){ m_Queue.stop(); });
        }

        void insert(const T& aItem) { m_Queue.insert(aItem); }
        bool idle() const           { return m_Queue.idle(1); }
    };

    template<class T>
    class DelayQueueThread
    {
        const std::function<void(T& t)> m_Handler;
        struct Node {
            time_t moment = 0;
            T data;
        };
        struct NodeCmp {
            bool operator()(const Node& l, const Node& r) const {
                return l.moment > r.moment;
            }
        };
        using Q = std::priority_queue<Node, std::vector<Node>, NodeCmp>;
        SafeQueueThread<Node, Q> m_Queue;
    public:
        template<class F> DelayQueueThread(F aHandler)
        : m_Handler(aHandler)
        , m_Queue([this](auto& x){ m_Handler(x.data); }) {}

        void start(Group& aGroup, unsigned count = 1)
        {
            m_Queue.start(aGroup, [this](auto& x) -> bool {
                return x.moment < ::time(nullptr);
            }, count);
        }

        void insert(time_t ts, const T& aItem) { m_Queue.insert({ts + ::time(nullptr), aItem}); }
        bool idle() const                      { return m_Queue.idle(); }
    };

}
