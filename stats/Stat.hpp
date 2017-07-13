#pragma once

#include <mutex>
#include <atomic>
#include <string>
#include <time.h>

namespace Stat
{

    struct Count
    {
        std::atomic<size_t> value {0};
        void update(size_t v) { value += v; }
        void set(size_t v) { value = v; }
        void format(std::ostream& os, const std::string& prefix) const { os << prefix << ' ' << value << ' ' << time(0) << std::endl; }
    };

    // flags: RPS, AGO
    struct Time
    {
        enum FLAGS {RPS = 1, AGO};
        const size_t flags = 0;

        std::mutex mutex;
        using Lock = std::unique_lock<std::mutex>;
        double min = 0;
        double max = 0;
        double time = 0;
        size_t count = 0;
        time_t last = 0;

        Time() {}
        Time(FLAGS f) : flags(f) {}

        void update(double v) { Lock lk(mutex); count++; time+=v; min = (min == 0 ? v : std::min(min, v));  max = std::max(max, v); }
        void set(double v) { Lock lk(mutex);  count=1; time = v;  min = v;  max = v; }

        // call with 1sec as minimal period if rps mode
        void format(std::ostream& os, const std::string& prefix)
        {
            time_t now = ::time(0);
            if (flags == AGO)
                os << prefix << "_ago" << ' ' << now - time << ' ' << now << std::endl;
            else
            {
                double avg = count > 0 ? time/(float)count : 0;
                os << prefix << "_min" << ' ' << min << ' ' << now << std::endl;
                os << prefix << "_max" << ' ' << max << ' ' << now << std::endl;
                os << prefix << "_avg" << ' ' << avg << ' ' << now << std::endl;
                if (flags == RPS)
                {
                    if (last > 0)
                    {
                        auto ela = now - last;
                        os << prefix << "_rps" << ' ' << (ela > 0 ? count / float(ela) : 0.f) << ' ' << now << std::endl;
                    }
                    else
                        os << prefix << "_rps" << ' ' << count << ' ' << now << std::endl;
                    last = now;
                    clear();
                }
                else
                {
                    os << prefix << "_count" << ' ' << count << ' ' << now << std::endl;
                }
            }
        }
        void clear() { min = 0; max = 0; time = 0; count = 0; }
    };

    // no flags
    struct Bool
    {
        std::atomic<bool> flag {0};

        void set() { flag = true; }
        void clear() { flag = false; }
        void format(std::ostream& os, const std::string& prefix) const {
            os << prefix << ' ' << flag << ' ' << time(0) << std::endl;
        }
    };

}
