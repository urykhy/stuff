#pragma once
#include <stddef.h>
#include <sys/time.h>
#include <cmath>

namespace Util {

    struct time_val : public ::timeval {
        time_val(time_t t = 0, suseconds_t us = 0) {
            tv_sec = t;
            tv_usec = us;
        }
        time_val(double t) {
            tv_sec = ceil(t);
            tv_usec = (t - tv_sec) * 1000000;
        }

        static time_val now() {
            struct time_val n;
            gettimeofday(&n, 0);
            return n;
        }

        bool is_null() const {
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
            return tv_sec < tv.tv_sec || (tv_sec == tv.tv_sec && tv_usec < tv.tv_usec);
        }
        bool operator>(const time_val& tv) const
        {
            return tv_sec > tv.tv_sec || (tv_sec == tv.tv_sec && tv_usec > tv.tv_usec);
        }
        uint64_t to_ms() const {
            return uint64_t(tv_sec) * 1000 + tv_usec / 1000;
        }
        uint64_t to_us() const {
            return uint64_t(tv_sec) * 1000000 + tv_usec;
        }
        double to_double() const {
            return tv_sec + tv_usec/1000000.0;
        }
    };
    inline std::ostream& operator<<(std::ostream &out, const time_val &t) {
        out << t.to_double();
        return out;
    }

    inline
    time_val
    get_time()
    {
        return time_val::now();
    }

    class TimeMeter {
        const time_val start;
    public:
        TimeMeter() : start(get_time())
        {
            ;;
        }
        time_val get() const
        {
            return get_time() - start;
        }
    };

    class TimeoutHelper
    {
        static const size_t MSEC = 1000;
        const time_val stop;
    public:
        explicit TimeoutHelper(const time_val& t)
        : stop(get_time() + t)
        {
            ;;
        }

        const time_val stop_time() const {
            return stop;
        }

        // get rest in ms (for poll)
        int rest() const
        {
            auto r = get_time();
            if (stop < r) {
                return 0;
            }
            return (stop - r).to_ms();
        }

        bool expired() const {
            return stop < get_time();
        }
    };

} // namespace Util
