#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

// via: https://github.com/mvorbrodt/blog/blob/master/src/base64.hpp
namespace Parser {

    enum Base64Flags
    {
        BASE64_NO_PADDING = 1,
        BASE64_URL_SAFE   = 2,
    };

    inline std::string Base64(const std::string_view& aStr, const unsigned aFlags = {})
    {
        unsigned char sPadCharacter = '=';
        unsigned char sPlus         = '+';
        unsigned char sSlash        = '/';
        if (aFlags & BASE64_URL_SAFE) {
            sPadCharacter = '.';
            sPlus         = '-';
            sSlash        = '_';
        }

        size_t padding{};

        if ((aFlags & BASE64_NO_PADDING) == 0) {
            if (aStr.size() % 4)
                throw std::invalid_argument("Format::Base64: invalid size");
            if (aStr.size() > 2) {
                if (aStr[aStr.length() - 1] == sPadCharacter)
                    padding++;
                if (aStr[aStr.length() - 2] == sPadCharacter)
                    padding++;
            }
        }

        std::string decoded;
        decoded.reserve(((aStr.size() / 4) * 3) - padding);

        std::uint32_t temp{};
        auto          it = aStr.begin();

        while (it < aStr.end()) {
            for (std::size_t i = 0; i < 4; ++i) {
                unsigned c = 0;
                if (it != aStr.end())
                    c = *it;
                else
                    c = sPadCharacter;

                temp <<= 6;
                if (c >= 'A' && c <= 'Z')
                    temp |= c - 'A';
                else if (c >= 'a' && c <= 'z')
                    temp |= c - 'a' + 26;
                else if (c >= '0' && c <= '9')
                    temp |= c - '0' + 52;
                else if (c == sPlus)
                    temp |= 0x3E;
                else if (c == sSlash)
                    temp |= 0x3F;
                else if (c == sPadCharacter) {
                    unsigned sPadSize = aStr.end() - it;
                    if (aFlags & BASE64_NO_PADDING) {
                        unsigned sExtra = aStr.size() % 4;
                        if (sExtra == 3)
                            sPadSize = 1;
                        else if (sExtra == 2)
                            sPadSize = 2;
                        else
                            throw std::invalid_argument("Format::Base64: invalid input");
                    }

                    switch (sPadSize) {
                    case 1:
                        decoded.push_back((temp >> 16) & 0x000000FF);
                        decoded.push_back((temp >> 8) & 0x000000FF);
                        return decoded;
                    case 2:
                        decoded.push_back((temp >> 10) & 0x000000FF);
                        return decoded;
                    default:
                        throw std::invalid_argument("Format::Base64: invalid padding");
                    }
                } else
                    throw std::invalid_argument("Format::Base64: invalid character: " + c);

                if (it != aStr.end())
                    it++;
            }

            decoded.push_back((temp >> 16) & 0x000000FF);
            decoded.push_back((temp >> 8) & 0x000000FF);
            decoded.push_back((temp)&0x000000FF);
        }

        return decoded;
    }
} // namespace Parser
