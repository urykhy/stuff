#pragma once

#include <string>
#include <string_view>
#include <fstream>

#include "Decompress.hpp"

namespace File
{
    inline std::string to_string(const std::string& aFilename)
    {
        std::string sBuf;
        read_file(aFilename, [&sBuf](auto& aStream)
        {
            sBuf.assign((std::istreambuf_iterator<char>(aStream)), std::istreambuf_iterator<char>());
        });
        return sBuf;
    }

    template<class T>
    void by_string(const std::string& aFilename, T aHandler)
    {
        read_file(aFilename, [aHandler = std::move(aHandler)](auto& aStream) mutable
        {
            std::string sBuf;
            try {
                while (std::getline(aStream, sBuf))
                    aHandler(sBuf);
            } catch (...) {
                if (!aStream.eof())
                    throw;
            }
        });
    }

    // aHandler must return number of bytes processed
    template<class T>
    inline void by_chunk(const std::string& aName, T aHandler, size_t aBufferSize = 1024 * 1024)
    {
        read_file(aName, [aHandler = std::move(aHandler), aBufferSize](auto& aStream)
        {
            uint64_t sOffset = 0;
            std::string sBuffer(aBufferSize, '\0');
            while (aStream.good())
            {
                aStream.read(sBuffer.data() + sOffset, aBufferSize - sOffset);
                auto sLen = aStream.gcount();
                auto sUsed = aHandler(std::string_view(sBuffer.data(), sLen + sOffset));
                if (sUsed == 0 and sLen + sOffset == aBufferSize)
                    throw std::runtime_error("File::by_chunk: no progress");
                sOffset = sLen + sOffset - sUsed;
                memmove(sBuffer.data(), sBuffer.data() + sUsed, sOffset);
            }
        });
    }
}
