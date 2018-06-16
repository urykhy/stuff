#pragma once
#include <cmath>
#include <iostream>
#include <Group.hpp>

namespace Threads
{
    void sleep(float f)
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

        template<class T>
        void thread_loop(unsigned aPeriod, T aHandler)
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
        template<class T>
        void start(Group& aGroup, unsigned aPeriod, T aHandler)
        {
            aGroup.start([aPeriod, aHandler, this](){
                thread_loop(aPeriod, aHandler);
            });
            aGroup.at_stop([this](){ m_Stop = true; });
        }
    };
}
