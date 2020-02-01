#pragma once

#include <uuid/uuid.h>
#include <parser/Hex.hpp>

namespace Util
{
    inline std::string Uuid()
    {
        uuid_t sID;
        uuid_generate_random(sID);
        const std::string sTmp((const char*)sID, sizeof(sID));
        return Parser::to_hex(sTmp);
    }

    using UuidPair = std::pair<uint64_t, uint64_t>;
    inline UuidPair Uuid64()
    {
        uuid_t sID;
        uuid_generate_random(sID);
        uint64_t* sPtr = (reinterpret_cast<uint64_t*>(&sID));
        return std::make_pair(htobe64(sPtr[0]), htobe64(sPtr[1]));
    }
} // namespace Util