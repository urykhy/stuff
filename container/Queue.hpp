#pragma once

#include <chrono>
#include <thread>
#include <vector>
#include <boost/lockfree/spsc_queue.hpp>

namespace Container
{
    template<class T>
    class Queue {
        std::vector<T> m_Data;
        boost::lockfree::spsc_queue<unsigned> m_Queue;
        const unsigned m_Max;
        unsigned m_Back = 0;
    public:

        Queue(unsigned aSize)
        : m_Data(aSize)
        , m_Queue(aSize - 2)    // 2 elements required to separate writer from reader
        , m_Max(aSize)
        { }

        bool empty() { return m_Queue.empty(); }

        // reading
        T* pop()
        {
            unsigned sFront = 0;
            if (m_Queue.pop(sFront))
                return &m_Data[sFront];
            return nullptr;
        }

        // writing
        T* current() {
            return &m_Data[m_Back];
        }

        // can sleep
        void push()
        {
            using namespace std::chrono_literals;
            while (!m_Queue.push(m_Back))
                std::this_thread::sleep_for(1ms);
            m_Back = (m_Back + 1) % m_Max;
        }
    };
}
