#pragma once
#include <string.h>

#include <sstream>
#include <stdexcept>

namespace Exception {
    inline std::string strerror(int e)
    {
        char sBuffer[1024];
#if (_POSIX_C_SOURCE >= 200112L) && !_GNU_SOURCE
        if (0 == strerror_r(e, sBuffer, sizeof(sBuffer)))
            return std::string(sBuffer);
        else
            return "strerror_r errror";
#else
        return strerror_r(e, sBuffer, sizeof(sBuffer));
#endif
    }

    template <class T, class B = std::runtime_error>
    struct Error : public B
    {
        Error(const std::string& aMessage)
        : B(aMessage)
        {}
    };

    struct ErrnoError : std::runtime_error
    {
        static std::string with_errno(const std::string& aMsg, int aErrno)
        {
            std::stringstream sBuffer;
            sBuffer << aMsg << ": " << strerror(aErrno) << " (" << aErrno << ")";
            return sBuffer.str();
        }
        const int m_Errno;

        ErrnoError(const std::string& aMsg, int aErrno = errno)
        : std::runtime_error(with_errno(aMsg, aErrno))
        , m_Errno(aErrno)
        {}
    };

    struct HttpError : std::runtime_error
    {
        static std::string with_http(const std::string& aMsg, int aStatus)
        {
            std::stringstream sBuffer;
            sBuffer << "Http response: " << aStatus << " (" << aMsg << ")";
            return sBuffer.str();
        }
        const unsigned m_Status;

        HttpError(const std::string& aMsg, unsigned aStatus)
        : std::runtime_error(with_http(aMsg, aStatus))
        , m_Status(aStatus)
        {}
    };

} // namespace Exception
