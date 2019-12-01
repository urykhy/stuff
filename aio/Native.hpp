 #pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <poll.h>
#include <string.h>

#include "Buffer.hpp"
#include "CTX.hpp"
#include <networking/EventFd.hpp>
#include <threads/Group.hpp>

namespace AIO
{
    using BufferPtr = std::shared_ptr<Buffer>;
    using Handler = std::function<void(int, BufferPtr)>;

    class Native
    {
        Native(const Native&) = delete;
        Native& operator=(const Native&) = delete;

        static __u64 pointer2u64(void* p) { return (__u64)(uintptr_t)p; }

        void prepare_pread(struct iocb *iocb, int fd, void *buf, ssize_t len, ssize_t offset)
        {
            memset(iocb, 0, sizeof(*iocb));
            iocb->aio_data = pointer2u64(buf);
            iocb->aio_fildes = fd;
            iocb->aio_lio_opcode = IOCB_CMD_PREAD;
            iocb->aio_reqprio = 0;
            iocb->aio_buf = pointer2u64(buf);
            iocb->aio_nbytes = len;
            iocb->aio_offset = offset;
            iocb->aio_flags = IOCB_FLAG_RESFD;
            iocb->aio_resfd = m_Event.get();
        }

        std::atomic_bool m_Running{true};
        AioCtx        m_Ctx;
        Util::EventFd m_Event;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        struct Info
        {
            Handler handler;
            BufferPtr buffer;
        };
        std::map<__u64, Info> m_Queue;
    public:

        Native() {}

        // memory must be aligned to page
        void read(int fd, ssize_t len, ssize_t offset, Handler aHandler)
        {
            const size_t io_page = 4096;
            size_t sLen = ((len / io_page) + (len % io_page ? 1 : 0)) * io_page;
            auto sBuffer = std::make_shared<Buffer>(sLen);
            {
                Lock lock(m_Mutex);
                m_Queue[pointer2u64(sBuffer->data())] = Info{aHandler, sBuffer};
            }

            struct iocb req;
            prepare_pread(&req, fd, sBuffer->data(), len, offset);
            m_Ctx.submit(&req);
        }

        void start(Threads::Group& aGroup)
        {
            aGroup.start([this](){ process(); });
            aGroup.at_stop([this](){ m_Running = false; });
        }
    private:

        void process()
        {
            pollfd pfd[1];
            memset(pfd, 0, sizeof(pfd));
            pfd[0].fd = m_Event.get();
            pfd[0].events = POLLIN;

            while (m_Running)
            {
                pfd[0].revents = 0;
                int rc = poll(pfd, 1, 100 /* ms */);
                if (rc > 0 and pfd[0].revents & POLLIN)
                {
                    m_Event.read();
                    process_notifications();
                }
            }
        }

        void process_notifications()
        {
            AioCtx::Events events;
            int rc = m_Ctx.get_events(events);
            if (rc <= 0)
                return;

            Lock lock(m_Mutex);
            for (int i = 0; i < rc; ++i)
            {
                auto it = m_Queue.find(events[i].data);
                if (it == m_Queue.end())
                    continue;
                // FIXME: call handler w/o lock
                it->second.handler(events[i].res, it->second.buffer);
                m_Queue.erase(it);
            }
        }
    };
}
