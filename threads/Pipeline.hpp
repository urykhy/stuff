#pragma once
#include <functional>
#include <list>
#include <queue>
#include <SafeQueue.hpp>

namespace Threads
{
    template<class T>
    class Pipeline
    {
        using Stages = std::list<std::function<void(T& t)>>;
        Stages stages;

        struct Node {
            T data;
            typename Stages::iterator current;
            uint64_t serial = 0;

            Node(T& t, typename Stages::iterator it, uint64_t s) : data(t), current(it), serial(s) {}
            Node() {}
        };

        struct NodeCmp
        {
            bool operator()(const Node& l, const Node& r) const
            {
                return l.serial > r.serial;
            }
        };

        using PriorityList = std::priority_queue<Node, std::vector<Node>, NodeCmp>;

        SafeQueueThread<Node, PriorityList> q;
        std::atomic<uint64_t> serial{0};

        void one_step(Node& n)
        {
            (*n.current)(n.data);
            n.current++;
            if (n.current != stages.end()) {
                q.insert(n);
            }
        }

    public:
        Pipeline() : q ([this](auto& a){ this->one_step(a); }) { }
        template<class F> void stage(F f) { stages.push_back(f); }
        void insert(T& t) { q.insert(Node{t, stages.begin(), serial++}); }

        void start(Group& tg, unsigned count = 1) {
            q.start(tg, [](Node&){ return true; }, count);
        }
        bool idle() const { return q.idle(); }
    };
}
