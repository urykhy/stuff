#pragma once

#include <uuid/uuid.h>
#include <format/Hex.hpp>

namespace Util
{
    inline std::string Uuid()
    {
        uuid_t sID;
        uuid_generate_random(sID);
        const std::string sTmp((const char*)sID, sizeof(sID));
        return Format::to_hex(sTmp);
    }

    inline auto Uuid64()
    {
        uuid_t sID;
        uuid_generate_random(sID);
        uint64_t* sPtr = (reinterpret_cast<uint64_t*>(&sID));
        return std::make_tuple(sPtr[0], sPtr[1]);
    }
} // namespace Util