#pragma once

#include <arpa/nameser.h>
#include <resolv.h>

#include <vector>

#include "Resolve.hpp"

namespace Util
{
    class SRV
    {
        struct __res_state m_State;
    public:
        SRV()
        {
            memset(&m_State, 0, sizeof(m_State));
            if (res_ninit(&m_State))
               throw Exception::ErrnoError("fail to init resolver");
        }

        using HostPort = std::pair<uint32_t, uint16_t>;

        struct Entry
        {
            uint16_t prio = 0;
            uint16_t weight = 0;
            uint16_t port = 0;
            std::string name;

            HostPort resolve() const { return HostPort{resolveName(name), port}; }
        };
        using EntrySet = std::vector<Entry>;

        EntrySet operator()(const std::string& aName)
        {
            EntrySet sResult;
            unsigned char sBuf[1024];

            int rc = res_nquery(&m_State, aName.c_str(), ns_c_in, ns_t_srv, sBuf, sizeof(sBuf));
            if (rc == -1)
                throw Exception::ErrnoError("resolver error");

            ns_msg sMsg;
            ns_initparse(sBuf, rc, &sMsg);
            int sCount = ns_msg_count(sMsg, ns_s_an);
            for (int i = 0; i < sCount; i++)
            {
                ns_rr sRecord;
                ns_parserr(&sMsg, ns_s_an, i, &sRecord);

                const u_char* rdata = ns_rr_rdata(sRecord);

                Entry sEntry;
                // https://code.woboq.org/userspace/glibc/resolv/ns_print.c.html
                sEntry.prio   = ns_get16(rdata); rdata += NS_INT16SZ;
                sEntry.weight = ns_get16(rdata); rdata += NS_INT16SZ;
                sEntry.port   = ns_get16(rdata); rdata += NS_INT16SZ;

                const u_char *msg = ns_msg_base(sMsg);
                size_t     msglen = ns_msg_size(sMsg);
                char sName[NS_MAXDNAME];
                rc = dn_expand(msg, msg + msglen, rdata, sName, sizeof(sName));
                sEntry.name = sName;
                sResult.push_back(sEntry);
            }

            std::sort(sResult.begin(), sResult.end(), [](const auto& x, const auto& y) {
                if (x.prio < y.prio)
                    return true;
                if (x.prio > y.prio)
                    return false;
                return x.weight > y.weight;
            });

            return sResult;
        }
    };
}