#pragma once

#include <string>

namespace Parser
{
    inline std::string from_url(const std::string& aData)
    {
        enum {
            STAGE_INITIAL = 0,
            STAGE_FIRST,
            STAGE_SECOND
        };

        std::string sResult;
        sResult.reserve(aData.size());

        uint8_t stage = 0;
        uint8_t decode = 0;

        for (const uint8_t i : aData)
        {
            switch (stage) {
                case STAGE_INITIAL:
                    if (i != '%')
                        sResult.push_back(i);
                    else
                        stage = STAGE_FIRST;
                    break;
                case STAGE_FIRST:
                    decode = (aux::restore(i) << 4);
                    stage = STAGE_SECOND;
                    break;
                case STAGE_SECOND:
                    decode += aux::restore(i);
                    sResult.push_back(decode);
                    decode = 0;
                    stage = STAGE_INITIAL;
                    break;
            }
        }

        return sResult;
    }
}