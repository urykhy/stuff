#pragma once

#include <string>

#include "Resolve.hpp"
#include "UdpSocket.hpp"
#include <boost/circular_buffer.hpp>

#include <container/Queue.hpp>
#include <threads/Group.hpp>

namespace Udp
{
    class Producer
    {
        Socket m_Socket;
    public:

        Producer(const std::string& aAddr, uint16_t aPort)
        : m_Socket(Util::resolveName(aAddr), aPort)
        { }

        void write(const void* aPtr, ssize_t aSize)
        {
            // FIXME: handle errors
            m_Socket.write(aPtr, aSize);
        }
    };

    template<class T>
    class Consumer
    {
        struct Item
        {
            T data;
            ssize_t size;
        };

        using Handler = std::function<void(Item& t)>;
        Socket m_Socket;
        Container::Queue<Item> m_Queue;  // FIXME: not thread safe
        Handler m_Handler;
        std::atomic_bool m_Running{true};

        void recv_thread()
        {
            using namespace std::chrono_literals;
            while (m_Running)
            {
                Item* sTmp = m_Queue.current();
                sTmp->size = 0;
                sTmp->size = m_Socket.read(&sTmp->data, sizeof(T)); // FIXME: handle errors
                if (sTmp->size > 0)
                    m_Queue.push(); // sleep if queue full
            }
        }
        void handle_thread()
        {
            using namespace std::chrono_literals;
            while (m_Running)
            {
                if (!m_Queue.empty())
                {
                    Item* sTmp = m_Queue.pop();
                    m_Handler(*sTmp);
                } else {
                    std::this_thread::sleep_for(1ms);
                }
            }
        }
    public:

        Consumer(ssize_t aMaxSize, uint16_t aPort, Handler aHandler)
        : m_Socket(aPort)
        , m_Queue(aMaxSize)
        , m_Handler(aHandler)
        {
            m_Socket.set_timeout();
        }

        void start(Threads::Group& aTg)
        {
            aTg.start([this](){ recv_thread(); });
            aTg.start([this](){ handle_thread(); });
            aTg.at_stop([this](){ m_Running = false; });
        }
    };
}