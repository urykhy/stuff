#pragma once

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <exception/Error.hpp>

namespace Util {

    // return number in range [0 ... aMax);
    inline unsigned randomInt(uint64_t aMax)
    {
        return drand48() * aMax;
    }

    inline std::string randomStr(int size = 8)
    {
        static const char gAlNum[] = "0123456789"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "abcdefghijklmnopqrstuvwxyz";
        std::string sResult;
        sResult.reserve(size);
        for (int i = 0; i < size; i++)
            sResult.push_back(gAlNum[randomInt(sizeof(gAlNum))]);
        return sResult;
    }

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
