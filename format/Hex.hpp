#pragma once

#include <string>

namespace Format
{
    namespace aux
    {
        static const char sDict[]={'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
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
}
