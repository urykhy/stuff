#pragma once

#include <string>

#include "Interface.hpp"

#include <exception/Error.hpp>

namespace Archive {
    inline std::string filter(const std::string& aStr, IFilter* aFilter)
    {
        const size_t MAX_INPUT_CHUNK = 64 * 1024;
        std::string  sResult;
        std::string  sBuffer(aFilter->estimate(MAX_INPUT_CHUNK), ' '); // tmp buffer

        size_t sInputPos = 0;
        while (sInputPos < aStr.size()) {
            auto sInputSize = std::min(aStr.size() - sInputPos, MAX_INPUT_CHUNK);
            auto sInfo      = aFilter->filter(aStr.data() + sInputPos, sInputSize, &sBuffer[0], sBuffer.size());
            sInputPos += sInfo.usedSrc;
            sResult.append(sBuffer.substr(0, sInfo.usedDst));
            if (sInfo.usedDst == 0 and sInfo.usedSrc == 0)
                throw std::logic_error("Archive::filter make no progress");
        }
        while (true) {
            auto sInfo = aFilter->finish(&sBuffer[0], sBuffer.size());
            sResult.append(sBuffer.substr(0, sInfo.usedDst));
            if (sInfo.done)
                break;
            if (sInfo.usedDst == 0)
                throw std::logic_error("Archive::finish make no progress");
        }
        return sResult;
    }
} // namespace Archive