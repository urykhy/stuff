#pragma once

#include <fcntl.h>
#include <stdlib.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include <exception/Error.hpp>

namespace Util {
    class Drand48
    {
        struct drand48_data m_Data;
        static long int     m_Seed;

    public:
        Drand48()
        {
            long int sSeed = m_Seed;
            if (sSeed == 0) {
                if (sizeof(sSeed) != getrandom(&sSeed, sizeof(sSeed), 0)) {
                    sSeed = gettid();
                }
            }
            srand48_r(sSeed, &m_Data);
        }
        static void seed(long int aSeed)
        {
            m_Seed = aSeed;
        }
        double drand48()
        {
            double sResult = 0;
            drand48_r(&m_Data, &sResult);
            return sResult;
        }
    };
    inline long int Drand48::m_Seed = 0;

    inline double drand48()
    {
        thread_local Drand48 sLocal;
        return sLocal.drand48();
    }

    inline void seed(unsigned aSeed = 123)
    {
        Drand48::seed(aSeed);
    }

    inline uint32_t random4()
    {
        return round(drand48() * std::numeric_limits<std::uint32_t>::max());
    }

    inline uint64_t random8()
    {
        return ((uint64_t)random4()) << 32 | random4();
    }

    // return number in range [0 ... aMax);
    inline uint32_t randomInt(uint32_t aMax)
    {
        return std::min(uint32_t(std::floor(drand48() * aMax)), aMax - 1);
    }

    inline std::string randomStr(uint32_t aSize = 8)
    {
        static const char sAlNum[] = "0123456789"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "abcdefghijklmnopqrstuvwxyz";
        std::string       sResult;
        sResult.reserve(aSize);
        for (uint32_t i = 0; i < aSize; i++)
            // off by one since sAlnum contains terminating zero
            sResult.push_back(sAlNum[randomInt(sizeof(sAlNum) - 1)]);
        return sResult;
    }

    // based on https://github.com/chinuy/zipf/blob/master/zipf.go
    class Zipf
    {
        std::vector<double> m_Dist;

    public:
        Zipf(unsigned aMax, double aAlpha = 1.0)
        {
            std::vector<double> sTmp(aMax, 0);
            for (unsigned i = 1; i < aMax + 1; i++)
                sTmp[i - 1] = 1.0 / std::pow(double(i), aAlpha);

            std::vector<double> sZeta(aMax + 1, 0);
            m_Dist.resize(aMax + 1);

            for (unsigned i = 1; i < aMax + 1; i++)
                sZeta[i] += sZeta[i - 1] + sTmp[i - 1];

            for (unsigned i = 0; i < aMax + 1; i++)
                m_Dist[i] = sZeta[i] / sZeta[aMax];
        }

        unsigned operator()() const
        {
            auto sIt = std::upper_bound(m_Dist.begin(), m_Dist.end(), drand48());
            return std::distance(m_Dist.begin(), sIt) - 1;
        }
    };
} // namespace Util
