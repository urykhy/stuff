#pragma once

#include <string>

namespace Parser
{
    namespace aux
    {
        inline char restore(const char a)
        {
            if (a >= '0' and a <= '9')
                return a - '0';
            if (a >= 'a' and a <= 'f')
                return a - 'a' + 10;
            if (a >= 'A' and a <= 'F')
                return a - 'A' + 10;
            throw std::runtime_error("bad hex value");
        }
    }

    inline std::string from_hex(const std::string& x)
    {
        std::string sResult;
        if (x.size() % 2 != 0)
            throw std::runtime_error("Parser::from_hex");
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
