#pragma once
#include <stddef.h>
#include <sys/time.h>

#include <chrono>
#include <cmath>

namespace Time {
    struct time_spec : public ::timespec
    {
        static constexpr uint64_t SCALE = 1000 * 1000 * 1000;

        time_spec(const ::timespec& t)
        {
            tv_sec  = t.tv_sec;
            tv_nsec = t.tv_nsec;
        }
        time_spec(const ::timeval& t)
        {
            tv_sec  = t.tv_sec;
            tv_nsec = t.tv_usec * 1000;
        }
        time_spec(time_t t = 0, long ns = 0)
        {
            tv_sec  = t;
            tv_nsec = ns;
        }
        time_spec(double t)
        {
            tv_sec  = floor(t);
            tv_nsec = (t - tv_sec) * SCALE;
        }

        static time_spec now()
        {
            struct timespec s;
            clock_gettime(CLOCK_REALTIME, &s);
            return time_spec(s.tv_sec, s.tv_nsec);
        }
        static time_spec steady()
        {
            struct timespec s;
            clock_gettime(CLOCK_MONOTONIC, &s);
            return time_spec(s.tv_sec, s.tv_nsec);
        }

        bool is_null() const
        {
            return tv_sec == 0 && tv_nsec == 0;
        }

        time_spec operator+(const time_spec& ts) const
        {
            const auto sNs = to_ns() + ts.to_ns();
            return {(time_t)(sNs / SCALE), long(sNs % SCALE)};
        }
        time_spec operator-(const time_spec& ts) const
        {
            const auto sNs = to_ns() - ts.to_ns();
            return {(time_t)(sNs / SCALE), long(sNs % SCALE)};
        }
        bool operator<(const time_spec& ts) const
        {
            return to_ns() < ts.to_ns();
        }
        bool operator>(const time_spec& ts) const
        {
            return to_ns() > ts.to_ns();
        }

        uint64_t to_ns() const
        {
            return tv_sec * SCALE + tv_nsec;
        }
        uint64_t to_us() const
        {
            return to_ns() / 1000;
        }
        uint64_t to_ms() const
        {
            return to_us() / 1000;
        }
        double to_double() const
        {
            return to_ns() / 1000000000.0;
        }
    };

    inline std::ostream& operator<<(std::ostream& out, const time_spec& t)
    {
        out << t.to_double();
        return out;
    }

    inline time_spec get_time() { return time_spec::now(); }

    // old style meter
    class Meter
    {
        time_spec m_Start;

    public:
        Meter()
        : m_Start(time_spec::steady())
        {}
        time_spec get() const { return time_spec::steady() - m_Start; }
        void      reset() { m_Start = time_spec::steady(); }
    };

    // new style, with <chrono>
    class XMeter
    {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    public:
        std::chrono::duration<unsigned, std::nano> duration() const
        {
            auto stop = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
        }
    };

    // check if deadline expired
    class Deadline
    {
        const time_spec m_Deadline;

    public:
        Deadline(time_spec aDeadline)
        : m_Deadline(time_spec::steady() + aDeadline)
        {}

        bool expired() const
        {
            return time_spec::steady() > m_Deadline;
        }
    };
} // namespace Time
