#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <sys/epoll.h>
#include <vector>

#include "EventFd.hpp"
#include <container/RequestQueue.hpp>
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
            virtual Result on_read(int) = 0;
            virtual Result on_write(int) = 0;
            virtual void   on_error(int) = 0;
            virtual Result on_timer(int) { return OK; };
            virtual int    get_fd() const { return -1; }
            virtual ~HandlerFace() {}
        };
        using HandlerPtr = std::shared_ptr<HandlerFace>;
        using WeakPtr    = std::weak_ptr<HandlerFace>;
        using Func       = std::function<void(EPoll*)>;
    private:

        std::atomic_bool m_Running{true};
        int m_Fd = -1;
        std::vector<struct epoll_event> m_Events;
        std::map<int, HandlerPtr> m_Handlers;   // fd to handler
        std::set<int> m_Retry; // retry call
        std::set<int> m_CleanupQueue;   // store unique fds

        struct EventHandler : HandlerFace
        {
            EPoll* m_Parent = nullptr;
            EventHandler(EPoll* aParent) : m_Parent(aParent) {}
            Result on_read(int)  override { return m_Parent->on_event(); }
            Result on_write(int) override { return Result::OK; }
            void   on_error(int) override {}
        };
        EventFd m_Event;
        HandlerPtr m_EventHandler;
        Threads::SafeQueue<Func> m_External;


        struct Backlog
        {
            enum ACTION { UNKNOWN, READ, TIMER };

            WeakPtr ptr;
            ACTION  action{};
            int     timer_id{};
        };

        void on_backlog(Backlog& aData)
        {
            auto sPtr = aData.ptr.lock();
            if (sPtr)
            {
                HandlerFace::Result sResult = HandlerFace::CLOSE;
                try {
                    switch (aData.action)
                    {
                        case Backlog::READ:  sResult = sPtr->on_read(sPtr->get_fd()); break;
                        case Backlog::TIMER: sResult = sPtr->on_timer(aData.timer_id); break;
                        default: assert(0);
                    }
                } catch (...) { /* already value close */}

                if (sResult == HandlerFace::CLOSE)
                {
                    sPtr->on_error(sPtr->get_fd());
                    m_CleanupQueue.insert(sPtr->get_fd());
                }
            }
        }
        container::RequestQueue<Backlog> m_Backlog;

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
                auto sResult = HandlerFace::CLOSE;
                try {
                    sResult = sFace->on_write(aFd);
                } catch (...) { sResult = HandlerFace::Result::CLOSE; }
                sClose |= sResult == HandlerFace::Result::CLOSE;
                sRetry |= sResult == HandlerFace::Result::RETRY;
            }
            if (!sClose and aEvent & EPOLLIN)
            {
                auto sResult = HandlerFace::CLOSE;
                try {
                    sResult = sFace->on_read(aFd);
                } catch (...) { sResult = HandlerFace::Result::CLOSE; }
                sClose |= sResult == HandlerFace::Result::CLOSE;
                sRetry |= sResult == HandlerFace::Result::RETRY;
            }

            if (sClose)
            {
                sFace->on_error(aFd);
                m_CleanupQueue.insert(aFd);
                return;
            }

            if (sRetry)
                m_Backlog.insert(Backlog{sFace, Backlog::READ}, 1);
        }
    public:

        using Error = Exception::ErrnoError;

        EPoll(const unsigned aMaxEvents = 1024)
        : m_Backlog([this](Backlog& aData){ on_backlog(aData); })
        {
            m_Events.resize(aMaxEvents);

            m_Fd = epoll_create1(EPOLL_CLOEXEC);
            if (m_Fd == -1)
                throw Error("fail to create epoll socket");

            m_EventHandler = std::make_shared<EventHandler>(this);
            insert(m_Event.get(), EPOLLIN, m_EventHandler);
        }
        ~EPoll() { close(m_Fd); }

        void dispatch(int aTimeoutMs = 1000)
        {
            // default timeout - 1000 ms
            // if timer/retry used - do not sleep more than timer.eta()
            const unsigned sTimeout = m_Backlog.eta(aTimeoutMs);

            int sCount = epoll_wait(m_Fd, m_Events.data(), m_Events.size(), sTimeout);
            if (sCount == -1 and errno != EINTR)
                throw Error("epoll_wait failed");

            // process socket events
            for(int i = 0; i < sCount; i++)
            {
                uint32_t sEvent = m_Events[i].events;
                int sFd = m_Events[i].data.fd;
                process(sFd, sEvent);
            }

            // process timer and retry events
            m_Backlog.on_timer();

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
            aGroup.at_stop([this](){
                m_Running = false;
                m_Event.signal(); // break epoll_wait
            });
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
            int rc = epoll_ctl(m_Fd, EPOLL_CTL_DEL, aFd, nullptr);
            m_Handlers.erase(sIt); // close socket here, after CTL_DEL
            if (rc == -1)
                throw Error("epoll_ctl/erase failed");
        }

        // thread safe way to insert/erase
        void post(Func aFunc)
        {
            m_External.insert(aFunc);
            m_Event.signal();
        }

        void schedule(WeakPtr aPtr, int aDelayMS, int aTimerID = 0)
        {
            m_Backlog.insert(Backlog{std::move(aPtr), Backlog::TIMER, aTimerID}, aDelayMS);
        }
    };

} // namespace Util