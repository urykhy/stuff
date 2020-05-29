#pragma once

#include <sys/eventfd.h>
#include <unistd.h>

#include <exception/Error.hpp>

namespace Util {
    class EventFd
    {
        EventFd(const EventFd&) = delete;
        EventFd& operator=(const EventFd&) = delete;
        int      m_Fd                      = -1;

    public:
        using Error = Exception::ErrnoError;
        EventFd()
        : m_Fd(-1)
        {
            m_Fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (m_Fd == -1) {
                throw Error("fail to create eventfd socket");
            }
        }

        ~EventFd() throw() { ::close(m_Fd); }

        bool signal(uint64_t v = 1)
        {
            ssize_t res = write(m_Fd, &v, sizeof(v));
            if (res == sizeof(v))
                return true;
            if (res == -1 && (errno != EINTR && errno != EAGAIN))
                throw Error("fail to signal event_fd");
            return false;
        }

        size_t read()
        {
            uint64_t v   = 0;
            ssize_t  res = ::read(m_Fd, &v, sizeof(v));
            if (res == sizeof(v))
                return v;
            if (res == -1 && (errno != EINTR && errno != EAGAIN))
                throw Error("fail to read eventfd");
            return 0;
        }

        // use with caution: you can get a fd to listen on
        int get() const { return m_Fd; }
    };
} // namespace Util