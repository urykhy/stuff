#pragma once

#include <sched.h>
#include <exception/Error.hpp>

namespace Util
{
    // bind to core and set realtime scheduler
    inline void setCore(int sCore, int sPriority = 1)
    {
        struct sched_param sParam;
        memset(&sParam, 0, sizeof(sParam));
        sParam.sched_priority = sPriority;
        if (sched_setscheduler(0, SCHED_RR, &sParam) < 0)
            throw Exception::ErrnoError("fail to set scheduler");

        cpu_set_t sMask;
        CPU_ZERO(&sMask);
        CPU_SET(sCore, &sMask);
        if (sched_setaffinity(0, sizeof(sMask), &sMask) < 0)
            throw Exception::ErrnoError("fail to set affinity");
    }

} // namespace Util