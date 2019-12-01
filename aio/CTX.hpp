#pragma once

#include <array>

#include "API.hpp"
#include <exception/Error.hpp>

namespace AIO
{
    class AioCtx
    {
        AioCtx(const AioCtx&) = delete;
        AioCtx& operator=(const AioCtx&) = delete;

        aio_context_t m_Ctx;
    public:

        using Error = Exception::ErrnoError;

        AioCtx(unsigned aSize = 256) : m_Ctx(0)
        {
            if (io_setup(aSize, &m_Ctx))
                throw Error("io_setup error");
        }

        ~AioCtx() throw () { io_destroy(m_Ctx); }

        void submit(struct iocb* sReq)
        {
            if (io_submit(m_Ctx, 1, &sReq) != 1)
                throw Error("io_submit error");
        }

        using Events =  std::array<io_event, 128>;

        int get_events(Events& aEvents)
        {
            int r = io_getevents(m_Ctx, 0, aEvents.size(), &aEvents[0], NULL);
            if (r < 0)
                throw Error("io_getevents error");
            return r;
        }
    };
}