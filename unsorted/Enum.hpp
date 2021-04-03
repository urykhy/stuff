#pragma once

#define _DECLARE_ENUM_TO_STRING(X, V)                                      \
    inline std::string_view to_string(X aValue)                            \
    {                                                                      \
        auto sIt = (V).find(aValue);                                       \
        if (sIt != (V).end())                                              \
            return sIt->second;                                            \
        throw std::invalid_argument(#X);                                   \
    }                                                                      \
    inline std::ostream& operator<<(std::ostream& aStream, const X aValue) \
    {                                                                      \
        aStream << to_string(aValue);                                      \
        return aStream;                                                    \
    }
