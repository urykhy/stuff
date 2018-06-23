#pragma once
#include <aio.h>
#include <string.h>
#include <sched.h>

#include <cassert>
#include <map>

namespace AIO
{
    class PosixAio
    {
        std::map<void*, struct aiocb> ioq;

    public:
        PosixAio(size_t threads = 1, size_t rq = 1)
        {
            struct aioinit c;
            memset(&c, 0, sizeof(c));
            c.aio_threads = threads;
            c.aio_num = rq;
            aio_init(&c);
        }

        void read(int fd, void* buf, size_t len, size_t offset)
        {
            struct aiocb cb;
            memset(&cb, 0, sizeof(cb));
            cb.aio_fildes = fd;
            cb.aio_lio_opcode = LIO_READ;
            cb.aio_buf = buf;
            cb.aio_nbytes = len;
            cb.aio_offset = offset;
            auto i = ioq.insert(std::make_pair(buf, cb));
            assert(i.second);
            int rc = aio_read(&i.first->second);
            assert(!rc);
        }
        // FIXME: make write + fsync

        ssize_t wait(void* buf)
        {
            auto i = ioq.find(buf);
            assert(i != ioq.end());
            while (aio_error(&i->second) == EINPROGRESS)
            {
                sched_yield();  // FIXME: sleep 1ms ?
            }
            ssize_t ret = aio_return(&i->second);
            ioq.erase(i);
            return ret;
        }

        //  FIXME: add lio_listio
    };
}
