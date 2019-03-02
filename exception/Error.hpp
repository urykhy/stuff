#pragma once
#include <stdexcept>
#include <sstream>
#include <string.h>

namespace Exception
{
    template<class T>
    std::string append_errno(T aMsg, int aCode)
    {
        std::stringstream sBuffer;
        sBuffer << aMsg << " (errno " << aCode << ": " << strerror(aCode) << ")";
        return sBuffer.str();
    }

    template<class T, class B = std::runtime_error>
    struct Error : public B
    {
        Error(const std::string aMessage) : B(aMessage) {}
        Error(const std::string aMessage, int aCode) : B(append_errno(aMessage, aCode)) {}
    };
}
