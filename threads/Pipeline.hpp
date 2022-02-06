#pragma once
#include <functional>
#include <queue>

#include "SafeQueue.hpp"

namespace Threads::Pipeline {

    struct Task
    {
        using Step = std::function<void()>;
        using Wrap = std::function<void(Step&&)>;

        virtual void operator()(Wrap&& aWrap) = 0;
        virtual ~Task(){};
    };

    class Manager
    {
        struct Node
        {
            uint64_t              serial = {};
            std::shared_ptr<Task> task   = {};
            Task::Step            step   = {};
        };

        struct NodeCmp
        {
            bool operator()(const Node& l, const Node& r) const
            {
                return l.serial > r.serial;
            }
        };

        using PriorityQueue = std::priority_queue<Node, std::vector<Node>, NodeCmp>;
        using Queue         = SafeQueueThread<Node, PriorityQueue>;
        Queue                 m_Queue;
        std::atomic<uint64_t> m_Serial{0};

        void step(Node& aNode)
        {
            if (aNode.step)
                aNode.step();
            aNode.task->operator()([this, &aNode](auto&& aStep) {
                m_Queue.insert(Node{aNode.serial, aNode.task, std::move(aStep)});
            });
        }

    public:
        Manager()
        : m_Queue([this](auto& a) { this->step(a); }, Queue::Params{.retry = true})
        {}

        void start(Group& aGroup, unsigned aCount = 1) { m_Queue.start(aGroup, aCount); }
        void insert(std::shared_ptr<Task> aTask)
        {
            uint64_t sSerial = m_Serial++;
            m_Queue.insert(Node{sSerial, aTask});
        }
        bool idle() const { return m_Queue.idle(); }
    };
} // namespace Threads::Pipeline
