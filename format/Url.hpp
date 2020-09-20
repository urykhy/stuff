#pragma once

#include <string>

namespace Format
{
    namespace aux
    {
        //static const char sReserved[]={'!','*','\'','(',')',';',':','@','&','=','+','$',',','/','?','#','[',']'};
        inline bool unreserved(const char c)
        {
            return (c >= 'A' and c <= 'Z') || (c >= 'a' and c <= 'z') || (c >= '0' and c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        }
    }

    inline std::string url_encode(const std::string& aData)
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