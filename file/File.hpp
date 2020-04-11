#pragma once

#include <string>
#include <string_view>
#include <fstream>

#include "Decompress.hpp"

namespace File
{
    inline std::string to_string(const std::string& aFilename, size_t aBufferSize = 1024 * 1024)
    {
        std::string sBuf;
        with_file(aFilename, [&sBuf](auto& aStream)
        {
            sBuf.assign((std::istreambuf_iterator<char>(aStream)), std::istreambuf_iterator<char>());
        }, aBufferSize);
        return sBuf;
    }

    template<class T>
    void by_string(const std::string& aFilename, T aHandler, size_t aBufferSize = 1024 * 1024)
    {
        with_file(aFilename, [aHandler = std::move(aHandler)](auto& aStream) mutable
        {
            std::string sBuf;
            try {
                while (std::getline(aStream, sBuf))
                    aHandler(sBuf);
            } catch (...) {
                if (!aStream.eof())
                    throw;
            }
        }, aBufferSize);
    }

    // aHandler must return number of bytes processed
    template<class T>
    inline void by_chunk(const std::string& aName, T aHandler, size_t aBufferSize = 1024 * 1024)
    {
        with_file(aName, [aHandler = std::move(aHandler), aBufferSize](auto& aStream)
        {
            uint64_t sOffset = 0;
            std::string sBuffer(aBufferSize, '\0');
            while (aStream.good())
            {
                aStream.read(sBuffer.data() + sOffset, aBufferSize - sOffset);
                auto sLen = aStream.gcount();
                auto sUsed = aHandler(std::string_view(sBuffer.data(), sLen + sOffset));
                sOffset = sLen + sOffset - sUsed;
                memmove(sBuffer.data(), sBuffer.data() + sUsed, sOffset);
            }
        }, aBufferSize);
    }
}
