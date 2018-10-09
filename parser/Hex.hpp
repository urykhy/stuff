#pragma once

#include <string>

namespace Parser
{
    namespace aux
    {
        static const char sDict[]={'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

        inline char restore(const char a)
        {
            if (a >= '0' and a <= '9')
                return a - '0';
            if (a >= 'a' and a <= 'f')
                return a - 'a' + 10;
            throw std::runtime_error("bad hex value");
        }
    }

    inline std::string to_hex(const std::string& x)
    {
        std::string sResult;
        sResult.reserve(x.size() * 2);
        for (const auto& i : x)
        {
            auto a1 = i >> 4;
            auto a2 = i & 0x0F;
            sResult.push_back(aux::sDict[a1]);
            sResult.push_back(aux::sDict[a2]);
        }
        return sResult;
    }

    inline std::string from_hex(const std::string& x)
    {
        std::string sResult;
        if (x.size() % 2 != 0)
            throw std::runtime_error("bad string size");
        sResult.reserve(x.size() / 2);
        for (unsigned i = 0; i < x.size() / 2; i++)
        {
            auto a1 = x[i * 2];
            auto a2 = x[i * 2 + 1];
            sResult.push_back((aux::restore(a1) << 4) + aux::restore(a2));
        }
        return sResult;
    }
}
