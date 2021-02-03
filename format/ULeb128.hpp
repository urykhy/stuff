#pragma once

#include <iostream>

namespace Format
{
    // unsigned only
    inline void uleb128(uint64_t aValue, std::ostream& aStream)
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
}
