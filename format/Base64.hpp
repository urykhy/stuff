#pragma once

#include <string>
#include <string_view>

// via: https://github.com/mvorbrodt/blog/blob/master/src/base64.hpp
namespace Format {

    enum Base64Flags
    {
        BASE64_NO_PADDING = 1,
        BASE64_URL_SAFE   = 2,
    };

    inline std::string Base64(const std::string_view& aStr, const unsigned aFlags = {})
    {
        constexpr char sNormalLookup[]  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        constexpr char sNormalPad       = '=';
        constexpr char sUrlSafeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        constexpr char sUrlSafePad      = '.';

        const char* sEncodeLookup = sNormalLookup;
        char        sPadCharacter = sNormalPad;

        if (aFlags & BASE64_URL_SAFE) {
            sEncodeLookup = sUrlSafeLookup;
            sPadCharacter = sUrlSafePad;
        }

        std::string encoded;
        encoded.reserve(((aStr.size() / 3) + (aStr.size() % 3 > 0)) * 4);

        uint32_t temp{};
        auto     it = reinterpret_cast<const unsigned char*>(&aStr[0]);

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
            if ((aFlags & BASE64_NO_PADDING) == 0)
                encoded.append(2, sPadCharacter);
            break;
        case 2:
            temp = (*it++) << 16;
            temp += (*it++) << 8;
            encoded.append(1, sEncodeLookup[(temp & 0x00FC0000) >> 18]);
            encoded.append(1, sEncodeLookup[(temp & 0x0003F000) >> 12]);
            encoded.append(1, sEncodeLookup[(temp & 0x00000FC0) >> 6]);
            if ((aFlags & BASE64_NO_PADDING) == 0)
                encoded.append(1, sPadCharacter);
            break;
        }

        return encoded;
    }
} // namespace Format
