#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <sys/epoll.h>
#include <vector>

#include "EventFd.hpp"
#include <threads/Group.hpp>
#include <threads/SafeQueue.hpp>
#include <exception/Error.hpp>

namespace Util
{
    struct EPoll
    {
        struct HandlerFace
        {
            enum Result { OK, RETRY, CLOSE };
            virtual Result on_read() = 0;
            virtual Result on_write() = 0;
            virtual void on_error() = 0;
            virtual ~HandlerFace() {}
        };
        using HandlerPtr = std::shared_ptr<HandlerFace>;
        using Func = std::function<void(EPoll*)>;
    private:

        std::atomic_bool m_Running{true};
        int m_Fd = -1;
        uint64_t m_Serial = 0;
        std::vector<struct epoll_event> m_Events;
        std::map<int, HandlerPtr> m_Handlers;   // fd to handler
        std::set<int> m_Retry; // retry call
        std::vector<int> m_RetryQueue;
        std::vector<int> m_CleanupQueue;

        struct EventHandler : HandlerFace
        {
            EPoll* m_Parent = nullptr;
            EventHandler(EPoll* aParent) : m_Parent(aParent) {}
            Result on_read() override { return m_Parent->on_event(); }
            Result on_write() override { return Result::OK; }
            void on_error() override {}
        };
        EventFd m_Event;
        HandlerPtr m_EventHandler;
        Threads::SafeQueue<Func> m_External;

        HandlerFace::Result on_event()
        {
            const size_t sCount = m_Event.read();
            for (size_t i = 0; i < sCount; i++)
            {
                auto sFunc = m_External.try_get();
                if (sFunc)
                    sFunc->operator()(this);
            }
            return m_External.idle() ? HandlerFace::Result::OK : HandlerFace::Result::RETRY;
        }

        void process(int aFd, uint32_t aEvent)
        {
            auto sIt = m_Handlers.find(aFd);
            if (sIt == m_Handlers.end())
                return;

            bool sClose = aEvent & (EPOLLHUP | EPOLLERR);
            bool sRetry = false; // user can ask to call on_read on next dispatch

            auto sFace = sIt->second;
            if (!sClose and aEvent & EPOLLOUT)
            {
                auto sResult = sFace->on_write();
                sClose |= sResult == HandlerFace::Result::CLOSE;
                sRetry |= sResult == HandlerFace::Result::RETRY;
            }
            if (!sClose and aEvent & EPOLLIN)
            {
                auto sResult = sFace->on_read();
                sClose |= sResult == HandlerFace::Result::CLOSE;
                sRetry |= sResult == HandlerFace::Result::RETRY;
            }

            if (sClose)
            {
                sFace->on_error();
                m_CleanupQueue.push_back(aFd);
                return;
            }

            if (sRetry)
                m_RetryQueue.push_back(aFd);
        }
    public:

        using Error = Exception::ErrnoError;

        EPoll(const unsigned aMaxEvents = 1024)
        {
            m_Events.resize(aMaxEvents);
            m_RetryQueue.reserve(aMaxEvents);
            m_CleanupQueue.reserve(aMaxEvents);

            m_Fd = epoll_create1(EPOLL_CLOEXEC);
            if (m_Fd == -1)
                throw Error("fail to create epoll socket");

            m_EventHandler = std::make_shared<EventHandler>(this);
            insert(m_Event.get(), EPOLLIN, m_EventHandler);
        }
        ~EPoll() { close(m_Fd); }

        void dispatch(int aTimeoutMs = 10)
        {
            m_Serial++;
            int sCount = epoll_wait(m_Fd, m_Events.data(), m_Events.size(), aTimeoutMs);
            if (sCount == -1 and errno != EINTR)
                throw Error("epoll_wait failed");

            // process socket events
            for(int i = 0; i < sCount; i++)
            {
                uint32_t sEvent = m_Events[i].events;
                int sFd = m_Events[i].data.fd;
                process(sFd, sEvent);

                // if event here - delete from retry set, do not call same handler twice
                auto sIt = m_Retry.find(sFd);
                if (sIt != m_Retry.end())
                    m_Retry.erase(sIt);
            }

            // process retry events
            for (auto x : m_Retry)
                process(x, EPOLLIN);
            m_Retry.clear();
            m_Retry.insert(m_RetryQueue.begin(), m_RetryQueue.end());
            m_RetryQueue.clear();

            // call cleanups
            for (auto x : m_CleanupQueue)
                erase(x);
            m_CleanupQueue.clear();
        }

        void start(Threads::Group& aGroup)
        {
            aGroup.start([this](){
                while (m_Running)
                    dispatch();
            });
            aGroup.at_stop([this](){ m_Running = false; });
        }

        void insert(int aFd, uint32_t aEvent, HandlerPtr aHandler) // EPOLLIN or EPOLLOUT
        {
            struct epoll_event sEvent;
            memset(&sEvent, 0, sizeof(sEvent));
            sEvent.events = aEvent | EPOLLHUP | EPOLLET;
            sEvent.data.fd = aFd;
            int rc = epoll_ctl(m_Fd, EPOLL_CTL_ADD, aFd, &sEvent);
            if (rc == -1)
                throw Error("epoll_ctl/insert failed");
            m_Handlers[aFd] = aHandler;
        }
        void erase(int aFd)
        {
            auto sIt = m_Handlers.find(aFd);
            if (sIt == m_Handlers.end())
                return;
            m_Handlers.erase(sIt);
            int rc = epoll_ctl(m_Fd, EPOLL_CTL_DEL, aFd, nullptr);
            if (rc == -1)
                throw Error("epoll_ctl/erase failed");
        }

        // thread safe way to insert/erase
        void post(Func aFunc)
        {
            m_External.insert(aFunc);
            m_Event.signal();
        }
    };

} // namespace Util