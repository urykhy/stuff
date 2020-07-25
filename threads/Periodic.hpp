#pragma once
#include <cmath>
#include <iostream>
#include "Group.hpp"

namespace Threads
{
    inline void sleep(float f)
    {
        const struct timespec sleep_time{
            time_t(std::floor(f)),
            long((f - std::floor(f)) * 1000000000)
        };
        nanosleep(&sleep_time, nullptr);
    }

    class Periodic
    {
        volatile bool m_Stop{false};
        using Handler = std::function<void()>;

        void thread_loop(unsigned aPeriod, const Handler& aHandler)
        {
            time_t sLastRun = 0;
            while (!m_Stop)
            {
                if (sLastRun + aPeriod > time(0))
                {
                    sleep(0.5);
                    continue;
                }

                try
                {
                    aHandler();
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Periodic: " << e.what() << std::endl;
                }
                sLastRun = time(0);
            }
        }

    public:

        void start(Group& aGroup, unsigned aPeriod, Handler aHandler, Handler aTerm = [](){})
        {
            aGroup.start([aPeriod, aHandler = std::move(aHandler), aTerm = std::move(aTerm), this](){
                thread_loop(aPeriod, aHandler);
                aTerm();
            });
            aGroup.at_stop([this](){ m_Stop = true; });
        }
    };
}
