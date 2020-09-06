#pragma once

#include <string>

// via: https://github.com/mvorbrodt/blog/blob/master/src/base64.hpp
namespace Format {

    inline std::string Base64(const std::string& aStr)
    {
        constexpr char sEncodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        constexpr char sPadCharacter   = '=';

        std::string encoded;
        encoded.reserve(((aStr.size() / 3) + (aStr.size() % 3 > 0)) * 4);

        std::uint32_t temp{};
        auto          it = aStr.begin();

        for (std::size_t i = 0; i < aStr.size() / 3; ++i) {
            temp = (*it++) << 16;
            temp += (*it++) << 8;
            temp += (*it++);
            encoded.append(1, sEncodeLookup[(temp & 0x00FC0000) >> 18]);
            encoded.append(1, sEncodeLookup[(temp & 0x0003F000) >> 12]);
            encoded.append(1, sEncodeLookup[(temp & 0x00000FC0) >> 6]);
            encoded.append(1, sEncodeLookup[(temp & 0x0000003F)]);
        }

        switch (aStr.size() % 3) {
        case 1:
            temp = (*it++) << 16;
            encoded.append(1, sEncodeLookup[(temp & 0x00FC0000) >> 18]);
            encoded.append(1, sEncodeLookup[(temp & 0x0003F000) >> 12]);
            encoded.append(2, sPadCharacter);
            break;
        case 2:
            temp = (*it++) << 16;
            temp += (*it++) << 8;
            encoded.append(1, sEncodeLookup[(temp & 0x00FC0000) >> 18]);
            encoded.append(1, sEncodeLookup[(temp & 0x0003F000) >> 12]);
            encoded.append(1, sEncodeLookup[(temp & 0x00000FC0) >> 6]);
            encoded.append(1, sPadCharacter);
            break;
        }

        return encoded;
    }
} // namespace Format
