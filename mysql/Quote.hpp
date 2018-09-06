#pragma once

#include <string>

namespace MySQL
{
    std::string Quote(const std::string aQuery)
    {
        std::string sResult;
        sResult.reserve(aQuery.size());
        for (const char a : aQuery)
            switch (a) {
                case '\\':
                case '\'':
                case '\"':
                case '\0':
                case '\n':
                case '\r':
                case '`':
                    sResult.push_back('\\');
                default:
                    sResult.push_back(a);
            }
        return sResult;
    }
}
