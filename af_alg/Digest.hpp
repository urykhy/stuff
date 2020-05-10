#pragma once

#include <sys/socket.h>
#include <linux/if_alg.h>

#include <unsorted/Raii.hpp>
#include <mpl/Mpl.hpp>

namespace af_alg
{
    class DigestImpl
    {
        int m_FD = -1;
        size_t m_Size;
    public:

        DigestImpl(const std::string& aName, size_t aSize, const std::string& aKey="")
        : m_Size(aSize)
        {
            int sFD = socket(AF_ALG, SOCK_SEQPACKET, 0);
            if (sFD == -1)
                throw Exception::ErrnoError("fail to open AF_ALG socket");

            Util::Raii sCleanup([&sFD]() { ::close(sFD); });

            struct sockaddr_alg sAddr;
            memset(&sAddr, 0, sizeof(sAddr));
            sAddr.salg_family = AF_ALG;
            strcpy((char*)sAddr.salg_type, "hash");
            strncpy((char*)sAddr.salg_name, aName.data(), sizeof(sAddr.salg_name)-1);
            int rc = bind(sFD, (struct sockaddr *)&sAddr, sizeof(sAddr));
            if (rc == -1)
                throw Exception::ErrnoError("fail to bind AF_ALG socket");

            if (!aKey.empty())
            {
                rc = setsockopt(sFD, SOL_ALG, ALG_SET_KEY, aKey.data(), aKey.size());
                if (rc == -1)
                    throw Exception::ErrnoError("fail to set_key on AF_ALG socket");
            }

            m_FD = accept(sFD, NULL, 0);
            if (m_FD == -1)
                throw Exception::ErrnoError("fail to accept on AF_ALG socket");
        }

        ~DigestImpl() throw() { ::close(m_FD); }

        template<class T>
        void append(const T& aInput)
        {
            size_t rc = send(m_FD, aInput.data(), aInput.size(), MSG_MORE);
            if (rc != aInput.size())
                throw Exception::ErrnoError("fail to send data into AF_ALG socket");
        }

        std::string get()
        {
            int rc = send(m_FD, 0, 0, 0); // finalize
            if (rc < 0)
                throw Exception::ErrnoError("fail to finalize digest");

            std::string sBuf(m_Size, '\0');
            rc = read(m_FD, &sBuf[0], sBuf.size());
            if (rc != (int)m_Size)
                throw Exception::ErrnoError("fail to read digest from AF_ALG socket");
            return sBuf;
        }
    };

    template<class... T>
    std::string Digest(DigestImpl& aImpl, T&&... aInput)
    {
        Mpl::for_each_argument([&](const auto& aInput){
            aImpl.append(aInput);
        }, aInput...);
        return aImpl.get();
    };
}