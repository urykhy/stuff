#pragma once
#include <functional>
#include <list>
#include <queue>
#include <SafeQueue.hpp>

namespace Threads
{
    template<class T>
    struct Pipeline
    {
        using Stages = std::list<std::function<void(T& t)>>;

        struct Node
        {
            T data;
            typename Stages::iterator current;
            uint64_t serial = 0;
            uint16_t step = 0;

            template<class XT>
            Node(XT&& t, typename Stages::iterator it, uint64_t s, uint16_t st = 0 )
            : data(t), current(it), serial(s), step(st) {}
            Node() {}
        };

        struct NodeCmp
        {
            bool operator()(const Node& l, const Node& r) const
            {
                return l.step < r.step ? true : l.serial > r.serial;
            }
        };

        using PriorityList = std::priority_queue<Node, std::vector<Node>, NodeCmp>;
    private:

        Stages m_Stages;
        SafeQueueThread<Node, PriorityList> m_Queue;
        std::atomic<uint64_t> m_Serial{0};

        void one_step(Node& n)
        {
            (*n.current)(n.data);
            n.current++;
            n.step++;
            if (n.current != m_Stages.end()) {
                m_Queue.insert(n);
            }
        }

    public:
        Pipeline() : m_Queue ([this](auto& a){ this->one_step(a); }) { }

        template<class F>
        void stage(F f)   { m_Stages.push_back(f); }
        void insert(T& t) { m_Queue.insert(Node{t, m_Stages.begin(), m_Serial++}); }

        void start(Group& tg, unsigned count = 1) { m_Queue.start(tg, count); }
        bool idle() const { return m_Queue.idle(); }
    };
}
