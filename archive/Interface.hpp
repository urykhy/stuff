#pragma once

#include <sys/types.h>

#include <memory>

namespace Archive {
    struct IFilter
    {
        struct Pair
        {
            size_t usedSrc = 0;
            size_t usedDst = 0;

            Pair& operator+=(const Pair& aOther)
            {
                usedSrc += aOther.usedSrc;
                usedDst += aOther.usedDst;
                return *this;
            }
        };
        struct Finish
        {
            size_t usedDst = 0;
            bool   done    = false;
        };

        // size of dst space required, to make progress
        virtual size_t estimate(size_t aSize) { return 0; }

        virtual Pair   filter(const char* aSrc, size_t aSrcLen, char* aDst, size_t aDstLen) = 0;
        virtual Finish finish(char* aDst, size_t aDstLen) { return {0, true}; }
        virtual ~IFilter(){};
    };
    using FilterPtr = std::unique_ptr<IFilter>;
} // namespace Archive