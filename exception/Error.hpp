#pragma once
#include <stdexcept>
#include <sstream>
#include <string.h>

namespace Exception
{
    inline std::string strerror(int e)
    {
        char sBuffer[1024];
#if (_POSIX_C_SOURCE >= 200112L) && !  _GNU_SOURCE
        if (0 == strerror_r(e, sBuffer, sizeof(sBuffer)))
            return std::string(sBuffer);
        else
            return "strerror_r errror";
#else
        return strerror_r(e, sBuffer, sizeof(sBuffer));
#endif
    }

    inline std::string with_errno(const std::string& aMsg, int aCode)
    {
        std::stringstream sBuffer;
        sBuffer << aMsg << ": " << strerror(aCode) << " (" << aCode << ")";
        return sBuffer.str();
    }

    template<class T, class B = std::runtime_error>
    struct Error : public B
    {
        Error(const std::string& aMessage) : B(aMessage) {}
        Error(const std::string& aMessage, int aCode) : B(with_errno(aMessage, aCode)) {}
    };

    struct ErrnoError : std::runtime_error { ErrnoError (const std::string& aMsg, int aCode = errno) : std::runtime_error(Exception::with_errno(aMsg, aCode)) {}};
}
