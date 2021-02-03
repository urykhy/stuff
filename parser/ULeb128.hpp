#pragma once

#include <iostream>

namespace Parser
{
    // unsigned only
    inline uint64_t uleb128(std::istream& aStream)
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
}
