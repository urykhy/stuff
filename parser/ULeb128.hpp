#pragma once

#include <iostream>

namespace Parser
{
    // unsigned only
    namespace ULEB128
    {
        inline void encode(uint64_t aValue, std::ostream& aStream)
        {
            unsigned char sByte = 0;
            while (aValue > 0)
            {
                sByte = aValue & 0x7F;
                aValue = aValue >> 7;
                if (aValue != 0)    // more bytes to follow
                    sByte |= 0x80;
                aStream.write((const char*)&sByte, 1);
            }
        }

        inline uint64_t decode(std::istream& aStream)
        {
            uint64_t sValue = 0;
            unsigned sShift = 0;
            unsigned char sByte = 0;
            while (true)
            {
                aStream.read((char*)&sByte, 1);
                sValue += (sByte & 0x7f) << sShift;
                sShift += 7;
                if (0 == (sByte & 0x80))
                    break;
            }
            return sValue;
        }
    };
};
