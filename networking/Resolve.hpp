#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string.h>
#include <string>

#include <exception/Error.hpp>
#include <unsorted/Raii.hpp>

namespace Util
{
    inline uint32_t resolveAddr(const std::string& aAddr)
    {
        struct in_addr sTmp;
        if (0 == inet_aton(aAddr.c_str(), &sTmp))
            throw std::runtime_error(Exception::with_errno("fail to convert address", errno));
        return sTmp.s_addr;
    }

    inline uint32_t resolveName(const std::string& aName)
    {
        struct addrinfo sHints;
        memset(&sHints, 0, sizeof(sHints));
        sHints.ai_family = AF_INET;
        struct addrinfo* sInfo = nullptr;
        Util::Raii sCleanup([&sInfo](){ freeaddrinfo(sInfo); });

        int rc = getaddrinfo(aName.c_str(), nullptr, &sHints, &sInfo);
        if (rc != 0) throw std::runtime_error(std::string("fail to resolve name: ") + gai_strerror(rc));

        for (auto sPtr = sInfo; sPtr != nullptr; sPtr = sPtr->ai_next)
        {
            const sockaddr_in* sAddrPtr = reinterpret_cast<const sockaddr_in*>(sPtr->ai_addr);
            return sAddrPtr->sin_addr.s_addr;
        }
        return 0;
    }
} // namespace Util