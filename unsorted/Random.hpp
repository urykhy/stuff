#pragma once

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include <exception/Error.hpp>

namespace Util {

    inline void seed(unsigned aSeed = 123)
    {
        srand(aSeed);
    }

    // return number in range [0 ... aMax);
    inline uint64_t randomInt(uint64_t aMax)
    {
        return std::min(uint64_t(std::floor(drand48() * aMax)), aMax - 1);
    }

    inline std::string randomStr(int size = 8)
    {
        static const char sAlNum[] = "0123456789"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "abcdefghijklmnopqrstuvwxyz";
        std::string sResult;
        sResult.reserve(size);
        for (int i = 0; i < size; i++)
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

    class Random
    {
        int m_Fd;

    public:
        Random()
        {
            const std::string fname = "/dev/urandom";

            m_Fd = open(fname.c_str(), O_RDONLY);
            if (m_Fd == -1)
                throw Exception::ErrnoError("fail to open urandom");
        }

        ~Random() { close(m_Fd); }

        std::string operator()(int size)
        {
            std::string res(size, '\0');
            int         rc = read(m_Fd, &res[0], size);
            if (rc != size)
                throw Exception::ErrnoError("fail to read from urandom");
            return res;
        }
    };
} // namespace Util
