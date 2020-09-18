#pragma once

#include <vector>

#include "SafeQueue.hpp"

namespace Threads
{
    //
    // perform task in threads
    // but pass to join function in order
    //

    template<class T>
    class OrderedWorker
    {
        struct Task
        {
            T data;
            size_t serial;
            bool operator<(const Task& r) const { return this->serial > r.serial; }
        };

        using PriorityQ = std::priority_queue<Task, std::vector<Task>>;

        SafeQueueThread<Task> m_Worker;
        SafeQueueThread<Task, PriorityQ> m_Join;
        size_t m_Serial = 0;
        size_t m_NextJoin = 0;
    public:

        using Handler = std::function<void(T&)>;
        using Joiner  = std::function<void(T&)>;

        OrderedWorker(Handler aHandler, Joiner aJoiner)
        : m_Worker([this, aHandler](Task& t){
            aHandler(t.data);
            m_Join.insert(std::move(t));
        })
        , m_Join([this, aJoiner](Task& t){
            aJoiner(t.data);
            m_NextJoin++;
        }, {.check = [this](const Task& t){ return t.serial == m_NextJoin; }})
        { }

        void start(Group& aGroup, int aCount)
        {
            m_Worker.start(aGroup, aCount);
            m_Join.start(aGroup);
        }

        void insert(const T& aItem)
        {
            m_Worker.insert(Task{aItem, m_Serial++});
        }
    };

} // namespace Threads
