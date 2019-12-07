#pragma once

#include <uuid/uuid.h>
#include <parser/Hex.hpp>

namespace Util
{
    std::string Uuid()
    {
        uuid_t sID;
        uuid_generate_random(sID);
        const std::string sTmp((const char*)sID, sizeof(sID));
        return Parser::to_hex(sTmp);
    }

    uint64_t Uuid64()
    {
        uuid_t sID;
        uuid_generate_random(sID);
        return *(reinterpret_cast<uint64_t*>(&sID));
    }
} // namespace Util