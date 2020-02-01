#pragma once

#include "Raii.hpp"
#include <string>
#include <cxxabi.h>

namespace Util
{
    inline std::string Demangle(const char* aName)
    {
        int sStatus = 0;
        auto* sTmp = abi::__cxa_demangle(aName, 0, 0, &sStatus);
        Util::Raii sCleanup([sTmp](){ free(sTmp); });
        return std::string(sStatus == 0 ? sTmp : aName);
    }
} // namespace Util