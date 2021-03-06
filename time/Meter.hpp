#pragma once
#include <stddef.h>
#include <sys/time.h>

#include <chrono>
#include <cmath>

namespace Time {
    struct time_val : public ::timeval
    {
        time_val(const ::timeval& t)
        {
            tv_sec  = t.tv_sec;
            tv_usec = t.tv_usec;
        }
        time_val(time_t t = 0, suseconds_t us = 0)
        {
            tv_sec  = t;
            tv_usec = us;
        }
        time_val(double t)
        {
            tv_sec  = floor(t);
            tv_usec = (t - tv_sec) * 1000000;
        }

        static time_val now()
        {
            struct time_val n;
            gettimeofday(&n, 0);
            return n;
        }

        bool is_null() const
        {
            return tv_sec == 0 && tv_usec == 0;
        }
        time_val operator+(const time_val& tv) const
        {
            time_val res;
            timeradd(this, &tv, &res);
            return res;
        }
        time_val operator-(const time_val& tv) const
        {
            time_val res;
            timersub(this, &tv, &res);
            return res;
        }
        bool operator<(const time_val& tv) const
        {
            return timercmp(this, &tv, <);
        }
        bool operator>(const time_val& tv) const
        {
            return timercmp(this, &tv, >);
        }
        uint64_t to_ms() const
        {
            return uint64_t(tv_sec) * 1000 + tv_usec / 1000;
        }
        uint64_t to_us() const
        {
            return uint64_t(tv_sec) * 1000000 + tv_usec;
        }
        double to_double() const
        {
            return tv_sec + tv_usec / 1000000.0;
        }
    };
    inline std::ostream& operator<<(std::ostream& out, const time_val& t)
    {
        out << t.to_double();
        return out;
    }

    inline time_val get_time() { return time_val::now(); }

    // old style meter
    class Meter
    {
        time_val m_Start;

    public:
        Meter()
        : m_Start(get_time())
        {}
        time_val get() const { return get_time() - m_Start; }
        void reset() { m_Start = get_time(); }
    };

    // new style, with <chrono>
    class XMeter
    {
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    public:
        std::chrono::duration<unsigned, std::nano> duration() const
        {
            auto stop = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
        }
    };

    // check if deadline expired
    class Deadline
    {
        const time_val m_Deadline;

    public:
        Deadline(time_val aDeadline)
        : m_Deadline(get_time() + aDeadline)
        {}

        bool expired() const
        {
            return get_time() > m_Deadline;
        }
    };
} // namespace Time
