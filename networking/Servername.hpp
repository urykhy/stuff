#pragma once

#include <netdb.h>
#include <unistd.h>

#include <string>

namespace Util {
    inline std::string Servername()
    {
        char sTmp[128];
        if (gethostname(sTmp, sizeof(sTmp)))
            return "unknown";

        struct hostent* h = gethostbyname(sTmp);
        if (h == nullptr)
            return "unknown";

        return h->h_name;
    }
} // namespace Util