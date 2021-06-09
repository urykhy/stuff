#pragma once

#include <sys/resource.h>
#include <sys/time.h>

#include "Metrics.hpp"

#include <file/Dir.hpp>
#include <time/Meter.hpp>

namespace Prometheus {

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
        , m_RSS("rss_bytes")
        , m_FDS("files_count")
        , m_Thr("threads_count")
        , m_User("cpu_seconds_total{mode=\"user\"}")
        , m_System("cpu_seconds_total{mode=\"system\"}")
        {}

        void update() override
        {
            uint64_t sRSS  = 0;
            FILE*    sFile = nullptr;
            sFile          = fopen("/proc/self/statm", "r");
            if (sFile) {
                fscanf(sFile, "%*s%lu", &sRSS);
                fclose(sFile);
            }
            m_RSS.set(sRSS * m_PageSize);
            m_FDS.set(File::countFiles("/proc/self/fd"));
            m_Thr.set(File::countFiles("/proc/self/task"));

            struct rusage sUsage;
            if (getrusage(RUSAGE_SELF, &sUsage) == 0) {
                m_User.set(::Time::time_spec(sUsage.ru_utime).to_double());
                m_System.set(::Time::time_spec(sUsage.ru_stime).to_double());
            }
        }
    };

} // namespace Prometheus
