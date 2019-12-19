#pragma once

#include <sys/timerfd.h>
#include <exception/Error.hpp>

namespace Util
{
    class TimerFd
    {
        TimerFd(const TimerFd&) = delete;
        TimerFd& operator=(const TimerFd&) = delete;
        int m_Fd = -1;
    public:

        using Error = Exception::ErrnoError;
        TimerFd(uint64_t aTimeoutMs = 10, bool aMulti = true)
        {
            m_Fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
            if (m_Fd == -1) { throw Error("fail to create timerfd socket"); }

            itimerspec sNew;
            memset(&sNew, 0, sizeof(sNew));
            if (aMulti)
                sNew.it_interval.tv_nsec = aTimeoutMs * 1000 * 1000;
            sNew.it_value.tv_nsec = aTimeoutMs * 1000 * 1000;
            if (timerfd_settime(m_Fd, 0, &sNew, nullptr) < 0) { throw Error("fail to setup timer"); }
        }

        ~TimerFd() throw() { ::close(m_Fd); }

        // get number of expirations
        size_t read()
        {
            uint64_t v = 0;
            ssize_t res = ::read(m_Fd, &v, sizeof(v));
            if (res == sizeof(v))
                return v;
            if (res == -1 && (errno != EINTR && errno != EAGAIN))
                throw Error("fail to read timerfd");
            return 0;
        }

        int get() const { return m_Fd; }
    };
} // namespace Util