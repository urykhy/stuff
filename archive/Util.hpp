#pragma once

#include <string>

#include <exception/Error.hpp>

#include "Interface.hpp"

namespace Archive {
    std::string filter(const std::string& aStr, IFilter* aFilter)
    {
        std::string sResult;
        std::string sBuffer(64 * 1024, ' '); // tmp buffer

        size_t sInputPos = 0;
        while (sInputPos < aStr.size()) {
            auto sInputSize = aStr.size() - sInputPos;
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