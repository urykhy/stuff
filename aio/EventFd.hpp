#pragma once
#include <unistd.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <cassert>

namespace Util {

    class EventFd
    {
        EventFd(const EventFd&);
        EventFd& operator=(const EventFd&);
        int fd;
    public:
        EventFd() : fd(-1)
        {
            fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            assert(fd != -1);
        }

        ~EventFd() throw()
        {
            close(fd);
        }

        bool signal(uint64_t v = 1)
        {
            ssize_t res = write(fd, &v, sizeof(v));
            if (res == sizeof(v))
                return true;
            if (res == -1 && (errno != EINTR && errno != EAGAIN))
                throw "eventfd: write";
            return false;
        }

        size_t read()
        {
            uint64_t v = 0;
            ssize_t res = ::read(fd, &v, sizeof(v));
            if (res == sizeof(v))
                return v;
            if (res == -1 && (errno != EINTR && errno != EAGAIN))
                throw "eventfd: read";
            return 0;
        }

        // use with caution: you can get a fd to listen on
        int get() const
        {
            return fd;
        }
    };
}
