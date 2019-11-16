#pragma once

#include <string>
#include <netdb.h>
#include <unistd.h>

namespace Util
{
    std::string Servername()
    {
        char sTmp[128];
        if (gethostname(sTmp, sizeof(sTmp)))
            return "unknown";

        struct hostent* h = nullptr;
        h = gethostbyname(sTmp);
        if (h == nullptr)
            return "unknown";

        return h->h_name;
    }
} // namespace Util