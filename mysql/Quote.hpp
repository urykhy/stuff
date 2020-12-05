#pragma once

#include <string>

namespace MySQL
{
    // does not corrupt UTF-8 string
    inline std::string Quote(const std::string aStr)
    {
        std::string sResult;
        sResult.reserve(aStr.size() * 2);

        for (const char a : aStr)
        {
            switch (a) {
                case '\\':
                case '\'':
                case '\"':
                case '\0':
                case '\n':
                case '\r':
                case '`':
                    sResult.push_back('\\');
                    [[fallthrough]];
                default:
                    sResult.push_back(a);
            }
        }

        return sResult;
    }
}
