#pragma once

#include <string>
#include <string_view>

namespace Parser {
    namespace aux {
        inline char restore(const char a)
        {
            if (a >= '0' and a <= '9')
                return a - '0';
            if (a >= 'a' and a <= 'f')
                return a - 'a' + 10;
            if (a >= 'A' and a <= 'F')
                return a - 'A' + 10;
            throw std::invalid_argument("bad hex value");
        }
    } // namespace aux

    inline std::string from_hex(std::string_view x)
    {
        std::string sResult;
        if (x.size() % 2 != 0)
            throw std::invalid_argument("Parser::from_hex");
        sResult.reserve(x.size() / 2);
        for (unsigned i = 0; i < x.size() / 2; i++) {
            auto a1 = x[i * 2];
            auto a2 = x[i * 2 + 1];
            sResult.push_back((aux::restore(a1) << 4) + aux::restore(a2));
        }
        return sResult;
    }

    template <class T>
    typename std::enable_if<std::is_integral_v<T>, T>::type from_hex(const std::string_view x)
    {
        T sValue{};
        const std::string sTmp = from_hex(x);
        if (sTmp.size() != sizeof(T))
            throw std::invalid_argument("Parser::from_hex");
        memcpy(&sValue, sTmp.data(), sizeof(T));
        return sValue;
    }

    inline std::string from_hex_mixed(const std::string& x)
    {
        std::string sResult;
        sResult.reserve(x.size());

        enum State
        {
            IGNORE,
            STARTED,
            CHAR1,
            CHAR2
        };
        unsigned sState = IGNORE;
        char     a1     = 0;
        char     a2     = 0;

        for (auto c : x) {
            switch (sState) {
            case IGNORE:
                if (c == '\\')
                    sState++;
                else
                    sResult.push_back(c);
                break;
            case STARTED:
                if (c == 'x')
                    sState++;
                else {
                    switch (c) {
                    case 'n': sResult.push_back('\n'); break;
                    case 't': sResult.push_back('\t'); break;
                    default: throw std::invalid_argument(std::string("unexpected: \\") + c);
                    }
                    sState = IGNORE;
                }
                break;
            case CHAR1:
                a1 = c;
                sState++;
                break;
            case CHAR2:
                a2 = c;
                sResult.push_back((aux::restore(a1) << 4) + aux::restore(a2));
                sState = IGNORE;
                break;
            }
        }

        if (sState != IGNORE)
            throw std::invalid_argument("unexpected: eol");

        return sResult;
    }
} // namespace Parser
