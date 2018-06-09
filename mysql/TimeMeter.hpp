#pragma once

#include <chrono>

namespace Util
{
    class TimeMeter
    {
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    public:

        std::chrono::duration<unsigned, std::nano> duration() const
        {
            auto stop = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::nanoseconds>( stop - start );
        }
    };
}
