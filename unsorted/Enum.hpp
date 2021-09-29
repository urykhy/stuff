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

#define _DECLARE_SET_TO_STRING(X, V)                                       \
    inline std::string to_string(X aValue)                                 \
    {                                                                      \
        const std::underlying_type_t<X> sOne = 1;                          \
        std::string                     sResult;                           \
        const size_t                    sSize  = 8 * sizeof(sOne);         \
        bool                            sFirst = true;                     \
        for (size_t x = 0; x < sSize; x++) {                               \
            const auto sCurrent = sOne << x;                               \
            if (aValue ^ sCurrent)                                         \
                continue;                                                  \
            if (!sFirst)                                                   \
                sResult.append(", ");                                      \
            auto sIt = V.find(static_cast<X>(sCurrent));                   \
            if (sIt == V.end())                                            \
                throw std::logic_error("to_string: incomplete set " #X);   \
            sResult.append(sIt->second);                                   \
            sFirst = false;                                                \
        }                                                                  \
        return sResult;                                                    \
    }                                                                      \
    inline std::ostream& operator<<(std::ostream& aStream, const X aValue) \
    {                                                                      \
        aStream << to_string(aValue);                                      \
        return aStream;                                                    \
    }

#define _DECLARE_ENUM_OPS(X)                                          \
    inline X operator|(X a, X b)                                      \
    {                                                                 \
        using U = std::underlying_type<X>::type;                      \
        return static_cast<X>(static_cast<U>(a) | static_cast<U>(b)); \
    }                                                                 \
    inline X& operator|=(X& a, X b)                                   \
    {                                                                 \
        a = a | b;                                                    \
        return a;                                                     \
    }                                                                 \
    inline X operator&(X a, X b)                                      \
    {                                                                 \
        using U = std::underlying_type<X>::type;                      \
        return static_cast<X>(static_cast<U>(a) & static_cast<U>(b)); \
    }                                                                 \
    inline X& operator&=(X& a, X b)                                   \
    {                                                                 \
        a = a & b;                                                    \
        return a;                                                     \
    }                                                                 \
    inline X operator~(X a)                                           \
    {                                                                 \
        using U = std::underlying_type<X>::type;                      \
        return static_cast<X>(~static_cast<U>(a));                    \
    }                                                                 \
    inline X operator^(X a, X b)                                      \
    {                                                                 \
        using U = std::underlying_type<X>::type;                      \
        return static_cast<X>(static_cast<U>(a) ^ static_cast<U>(b)); \
    }                                                                 \
    inline X& operator^=(X& a, X b)                                   \
    {                                                                 \
        a = a ^ b;                                                    \
        return a;                                                     \
    }

/* avoid backslash-newline at end of file */