#pragma once
#include <map>
#include <vector>
#include <iostream>
#include <cassert>
#include <memory>

#include <sys/syscall.h>
#include <linux/aio_abi.h>
#include <aio.h>
#include <poll.h>

#include <string.h>
#include <EventFd.hpp>
#include <Expected.hpp>

extern "C" {
    static inline long io_setup(unsigned nr_reqs, aio_context_t *ctx)   { return syscall(__NR_io_setup, nr_reqs, ctx); }
    static inline long io_destroy(aio_context_t ctx)                    { return syscall(__NR_io_destroy, ctx); }
    static inline long io_submit(aio_context_t ctx, long n, struct iocb **paiocb) { return syscall(__NR_io_submit, ctx, n, paiocb); }
    static inline long io_cancel(aio_context_t ctx, struct iocb *aiocb, struct io_event *res) { return syscall(__NR_io_cancel, ctx, aiocb, res); }
    static inline long io_getevents(aio_context_t ctx, long min_nr, long nr, struct io_event *events, struct timespec *tmo) { return syscall(__NR_io_getevents, ctx, min_nr, nr, events, tmo); }
}

namespace AIO {

    namespace aux
    {
        class AioCtx
        {
            AioCtx(const AioCtx&);
            AioCtx& operator=(const AioCtx&);
            aio_context_t ctx;
        public:

            AioCtx(unsigned nr = 256) : ctx(0)
            {
                if (io_setup(nr, &ctx))
                    throw "fail to io_setup";
            }

            ~AioCtx() throw () {
                io_destroy(ctx);
            }

            void submit(struct iocb* req)
            {
                struct iocb *preq = req;
                if (io_submit(ctx, 1, &preq) != 1)
                    throw "fail to io_submit";
            }

            typedef std::vector<struct io_event> EventsT;

            int get_events(EventsT& events)
            {
                int r = io_getevents(ctx, 0, events.size(), &events[0], NULL);
                if (r < 0)
                    throw "fail to io_getevents";
                return r;
            }
        };

        // page aligned memory block
        class VABuffer
        {
            VABuffer(const VABuffer&) = delete;
            VABuffer& operator=(const VABuffer&) = delete;

            unsigned char* buffer = 0;
            size_t len = 0;
        public:
            VABuffer(size_t l) : len(l)
            {
                buffer = (unsigned char*)valloc(l);
                if (!buffer)
                    throw "valloc";
            }
            ~VABuffer() { free(buffer); }
            void* data() const { return buffer; }
            size_t size() const { return len;   }
            unsigned char data(size_t p) const { assert(p < len); return buffer[p];}
        };
    } // namespace aux

    struct NativeReader : public Util::Future<__s64>, aux::VABuffer
    {
        NativeReader(size_t sz) : aux::VABuffer(sz) { ;; }
        virtual ~NativeReader() throw() {}
    };
    typedef std::shared_ptr<NativeReader> NativeReaderPtr;

    class NativeAio
    {
        NativeAio(const NativeAio&);
        NativeAio& operator=(const NativeAio&);
        aux::AioCtx ctx;
        Util::EventFd event_;
        Util::EventFd term_;
        std::thread thread_;

        static __u64 pointer2u64(void* p)
        {
            return (__u64)(uintptr_t)p;
            //uintptr_t * pp = reinterpret_cast<uintptr_t *>(&p);
            //return (__u64)*pp;
        }

        static void asyio_prep_pread(struct iocb *iocb, int fd, void *buf,
                                     ssize_t nr_segs, ssize_t offset, int afd)
        {
            memset(iocb, 0, sizeof(*iocb));
            iocb->aio_data = pointer2u64(buf);
            iocb->aio_fildes = fd;
            iocb->aio_lio_opcode = IOCB_CMD_PREAD;
            iocb->aio_reqprio = 0;
            iocb->aio_buf = pointer2u64(buf);
            iocb->aio_nbytes = nr_segs;
            iocb->aio_offset = offset;
            iocb->aio_flags = IOCB_FLAG_RESFD;
            iocb->aio_resfd = afd;
        }

        typedef std::unique_lock<std::mutex> Lock;
        mutable std::mutex mutex;
        std::map<__u64, NativeReaderPtr> queue;

    public:
        NativeAio() { ;; }

        ~NativeAio() throw() { term(); }
        const size_t io_page = 4096;

        // FIXME: add cancel call

        // memory must be aligned to page
        NativeReaderPtr read(int fd, ssize_t len, ssize_t offset)
        {
            size_t blen = ((len / io_page) + (len % io_page ? 1 : 0)) * io_page;
            auto ptr = std::make_shared<NativeReader>(blen);
            {
                Lock guarg(mutex);
                queue[pointer2u64(ptr->data())] = ptr;
            }

            struct iocb req;
            asyio_prep_pread(&req, fd, ptr->data(), len, offset, event_.get());
            ctx.submit(&req);
            return ptr;
        }

        size_t pending() const
        {
            Lock guarg(mutex);
            return queue.size();
        }

        void run() {
            thread_ = std::thread(std::bind(&NativeAio::process_events, this));
        }

        void term()
        {
            if (thread_.joinable())
            {
                term_.signal();
                thread_.join();
            }
        }

    private:
        void process_events()
        {
            pollfd pfd[2];
            memset(pfd, 0, sizeof(pfd));
            pfd[0].fd = event_.get();
            pfd[0].events = POLLIN;
            pfd[1].fd = term_.get();
            pfd[1].events = POLLIN;

            while(1)
            {
                int rc = poll(pfd, 2, -1);
                if (rc < 0) {
                    std::cerr << "NativeAio poll error: " << errno << std::endl;
                    return;
                } else if (rc > 0) {
                    if (pfd[0].revents & POLLIN) {
                        process_notifications();
                        pfd[0].revents = 0;
                        event_.read();
                    }
                    if (pfd[1].revents) {   // terminating
                        pfd[0].revents = 0;
                        term_.read();
                        return;
                    }
                }
            }
        }

        void process_notifications()
        {
            aux::AioCtx::EventsT events(128);
            int rc = ctx.get_events(events);
            if (rc > 0)
            {
                Lock guarg(mutex);
                for(int i = 0; i < rc; ++i)
                {
                    auto it = queue.find(events[i].data);
                    assert (it != queue.end());
                    it->second->set_value(events[i].res);
                    queue.erase(it);
                }
            }
        }

    };
}
