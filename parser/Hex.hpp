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
            if (a >= 'A' and a <= 'F')
                return a - 'A' + 10;
            throw std::runtime_error("bad hex value");
        }
    }

    inline std::string to_hex(const std::string& x)
    {
        std::string sResult;
        sResult.reserve(x.size() * 2);
        for (const uint8_t i : x)
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

    inline std::string to_hex_c_string(const std::string& aData)
    {
        std::string sResult;
        sResult.reserve(aData.size() * 4);

        for (const uint8_t i : aData)
        {
            auto a1 = i >> 4;
            auto a2 = i & 0x0F;
            sResult.push_back('\\');
            sResult.push_back('x');
            sResult.push_back(aux::sDict[a1]);
            sResult.push_back(aux::sDict[a2]);
        }

        return sResult;
    }

    namespace aux
    {
        //static const char sReserved[]={'!','*','\'','(',')',';',':','@','&','=','+','$',',','/','?','#','[',']'};
        inline bool unreserved(const char c)
        {
            return (c >= 'A' and c <= 'Z') || (c >= 'a' and c <= 'z') || (c >= '0' and c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        }
    }

    inline std::string to_url(const std::string& aData)
    {
        std::string sResult;
        sResult.reserve(aData.size() * 3);
        for (const uint8_t i : aData)
        {
            if (aux::unreserved(i))
                sResult.push_back(i);
            else
            {
                auto a1 = i >> 4;
                auto a2 = i & 0x0F;
                sResult.push_back('%');
                sResult.push_back(aux::sDict[a1]);
                sResult.push_back(aux::sDict[a2]);
            }
        }
        return sResult;
    }

    inline std::string from_url(const std::string& aData)
    {
        enum {
            STAGE_INITIAL = 0,
            STAGE_FIRST,
            STAGE_SECOND
        };

        std::string sResult;
        sResult.reserve(aData.size());

        uint8_t stage = 0;
        uint8_t decode = 0;

        for (const uint8_t i : aData)
        {
            switch (stage) {
                case STAGE_INITIAL:
                    if (i != '%')
                        sResult.push_back(i);
                    else
                        stage = STAGE_FIRST;
                    break;
                case STAGE_FIRST:
                    decode = (aux::restore(i) << 4);
                    stage = STAGE_SECOND;
                    break;
                case STAGE_SECOND:
                    decode += aux::restore(i);
                    sResult.push_back(decode);
                    decode = 0;
                    stage = STAGE_INITIAL;
                    break;
            }
        }

        return sResult;
    }
}
