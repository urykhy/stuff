#pragma once

#include <string>
#include <string_view>

namespace Format {
    namespace aux {
        static const char sDict[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    }

    inline std::string to_hex(std::string_view x)
    {
        std::string sResult;
        sResult.reserve(x.size() * 2);
        for (const uint8_t i : x) {
            auto a1 = i >> 4;
            auto a2 = i & 0x0F;
            sResult.push_back(aux::sDict[a1]);
            sResult.push_back(aux::sDict[a2]);
        }
        return sResult;
    }

    template <class T>
    typename std::enable_if<std::is_integral_v<T>, std::string>::type to_hex(const T& aInput)
    {
        return to_hex(std::string_view((const char*)&aInput, sizeof(aInput)));
    }

    inline std::string to_hex_c_string(const std::string& aData)
    {
        std::string sResult;
        sResult.reserve(aData.size() * 4);

        for (const uint8_t i : aData) {
            auto a1 = i >> 4;
            auto a2 = i & 0x0F;
            sResult.push_back('\\');
            sResult.push_back('x');
            sResult.push_back(aux::sDict[a1]);
            sResult.push_back(aux::sDict[a2]);
        }

        return sResult;
    }

} // namespace Format
