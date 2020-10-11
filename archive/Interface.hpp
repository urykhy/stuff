#pragma once

#include <sys/types.h>

namespace Archive {
    struct IFilter
    {
        struct Pair
        {
            size_t usedSrc = 0;
            size_t usedDst = 0;
        };
        struct Finish
        {
            size_t usedDst = 0;
            bool   done    = false;
        };
        virtual Pair   filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) = 0;
        virtual Finish finish(char* aDst, size_t aDstLen) { return {0, true}; }
        virtual ~IFilter(){};
    };
} // namespace Archive