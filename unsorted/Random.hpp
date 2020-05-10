#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <exception/Error.hpp>

namespace Util
{
    class Random
    {
        int m_Fd;

    public:
        Random()
        {
            const std::string fname="/dev/urandom";
            m_Fd = open(fname.c_str(), O_RDONLY);
            if (m_Fd == -1)
                throw Exception::ErrnoError("fail to open urandom");
        }

        ~Random() throw () { close(m_Fd); }

        std::string operator()(int size)
        {
            std::string res(size, '\0');
            int rc = read(m_Fd, &res[0], size);
            if (rc != size)
                throw Exception::ErrnoError("fail to read from urandom");
            return res;
        }
    };
}