#pragma once

#include <stdexcept>
#include <string>

// via: https://github.com/mvorbrodt/blog/blob/master/src/base64.hpp
namespace Parser {

    inline std::string Base64(const std::string& aStr)
    {
        constexpr char sPadCharacter = '=';

        if (aStr.size() % 4)
            throw std::invalid_argument("Format::Base64: invalid size");

        std::size_t padding{};

        if (aStr.size() > 2) {
            if (aStr[aStr.length() - 1] == sPadCharacter)
                padding++;
            if (aStr[aStr.length() - 2] == sPadCharacter)
                padding++;
        }

        std::string decoded;
        decoded.reserve(((aStr.length() / 4) * 3) - padding);

        std::uint32_t temp{};
        auto          it = aStr.begin();

        while (it < aStr.end()) {
            for (std::size_t i = 0; i < 4; ++i) {
                temp <<= 6;
                if (*it >= 0x41 && *it <= 0x5A)
                    temp |= *it - 0x41;
                else if (*it >= 0x61 && *it <= 0x7A)
                    temp |= *it - 0x47;
                else if (*it >= 0x30 && *it <= 0x39)
                    temp |= *it + 0x04;
                else if (*it == 0x2B)
                    temp |= 0x3E;
                else if (*it == 0x2F)
                    temp |= 0x3F;
                else if (*it == sPadCharacter) {
                    switch (aStr.end() - it) {
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
                    throw std::invalid_argument("Format::Base64: invalid character");

                ++it;
            }

            decoded.push_back((temp >> 16) & 0x000000FF);
            decoded.push_back((temp >> 8) & 0x000000FF);
            decoded.push_back((temp)&0x000000FF);
        }

        return decoded;
    }
} // namespace Parser
