#pragma once

#include "SafeQueue.hpp"

namespace Threads
{
    // queue to process tasks at specific moments
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
            m_Queue.start(aGroup, count, [this](const auto& x) -> bool {
                return x.moment < ::time(nullptr);
            });
        }

        void insert(time_t ts, const T& aItem) { m_Queue.insert({ts + ::time(nullptr), aItem}); }
        bool idle() const                      { return m_Queue.idle(); }
    };
}