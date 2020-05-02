#pragma once

#include <sys/time.h>
#include <sys/resource.h>

#include <time/Meter.hpp>
#include <file/List.hpp>
#include "Stat.hpp"

namespace Stat
{
    struct Common : public ComplexFace
    {
        const uint64_t m_PageSize;
        Counter<>      m_RSS;
        Counter<>      m_FDS;
        Counter<>      m_Thr;
        Counter<float> m_User;
        Counter<float> m_System;

        Common()
        : m_PageSize(sysconf(_SC_PAGESIZE))
        , m_RSS("common.rss", "rss_bytes")
        , m_FDS("common.files", "open_files_count")
        , m_Thr("common.threads", "threads_count")
        , m_User("common.user_time", "cpu_seconds_total{mode=\"user\"}")
        , m_System("common.system_time", "cpu_seconds_total{mode=\"system\"}")
        {}

        void update() override
        {
            uint64_t sRSS = 0;
            FILE* fp = nullptr;
            fp = fopen("/proc/self/statm", "r");
            if (fp)
            {
                fscanf(fp, "%*s%lu", &sRSS);
                fclose(fp);
            }
            m_RSS.set(sRSS * m_PageSize);
            m_FDS.set(File::CountDir("/proc/self/fd"));
            m_Thr.set(File::CountDir("/proc/self/task"));

            struct rusage sUsage;
            if (getrusage(RUSAGE_SELF, &sUsage) == 0)
            {
                m_User.set(::Time::time_val(sUsage.ru_utime).to_double());
                m_System.set(::Time::time_val(sUsage.ru_stime).to_double());
            }
        }
    };

} // namespace Stat