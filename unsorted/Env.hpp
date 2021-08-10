#pragma once

#include <stdlib.h>

#include <string>
#include <string_view>

#include <parser/Atoi.hpp>

namespace Util {

    inline std::string getEnv(const char* aName)
    {
        std::string sTmp;
        if (auto sPtr = getenv(aName); sPtr != nullptr)
            sTmp.assign(sPtr);
        return sTmp;
    }

    template <class T>
    typename std::enable_if<std::is_integral_v<T>, T>::type getEnv(const char* aName, T aDefault = {})
    {
        if (auto sPtr = getenv(aName); sPtr != nullptr)
            return Parser::Atoi<T>(sPtr);
        return aDefault;
    }

} // namespace Util