#pragma once

#include <stdlib.h>

#include <cassert>
#include <regex>
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

    inline std::string expandEnv(const std::string& aStr)
    {
        static std::regex           sExpr("\\$\\{([^}]+)\\}");
        std::string                 sResult;
        std::string::const_iterator sIt = aStr.begin(), sEnd = aStr.end();
        for (std::smatch sMatch; std::regex_search(sIt, sEnd, sMatch, sExpr); sIt = sMatch[0].second) {
            assert(sMatch.size() == 2);
            sResult += sMatch.prefix();
            sResult += getEnv(sMatch[1].str().c_str());
        }
        sResult.append(sIt, sEnd);
        return sResult;
    }

} // namespace Util