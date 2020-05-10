#pragma once

#include <sys/socket.h>
#include <linux/if_alg.h>

namespace af_alg
{
    struct CryptoCfg
    {
        std::string name;
        std::string iv;
        std::string password;
        bool encrypt = true; // false to decrypt
        int tag_size = 0;
    };

    class CryptoImpl
    {
        const bool m_Encrypt;
        const int m_TagSize;
        int m_FD = -1;

    public:

        CryptoImpl(const CryptoCfg& aCfg)
        : m_Encrypt(aCfg.encrypt)
        , m_TagSize(aCfg.tag_size)
        {
            int sFD = socket(AF_ALG, SOCK_SEQPACKET, 0);
            if (sFD == -1)
                throw Exception::ErrnoError("fail to open AF_ALG socket");

            Util::Raii sCleanup([&sFD]() { ::close(sFD); });

            struct sockaddr_alg sAddr;
            memset(&sAddr, 0, sizeof(sAddr));
            sAddr.salg_family = AF_ALG;
            if (m_TagSize > 0)
                strcpy((char*)sAddr.salg_type, "aead");
            else
                strcpy((char*)sAddr.salg_type, "skcipher");
            strncpy((char*)sAddr.salg_name, aCfg.name.data(), sizeof(sAddr.salg_name)-1);
            int rc = bind(sFD, (struct sockaddr *)&sAddr, sizeof(sAddr));
            if (rc == -1)
                throw Exception::ErrnoError("fail to bind AF_ALG socket");

            rc = setsockopt(sFD, SOL_ALG, ALG_SET_KEY, aCfg.password.data(), aCfg.password.size());
            if (rc == -1)
                throw Exception::ErrnoError("fail to set key on AF_ALG socket");

            if (m_TagSize > 0)
            {
                rc = setsockopt(sFD, SOL_ALG, ALG_SET_AEAD_AUTHSIZE, &aCfg.tag_size, sizeof(aCfg.tag_size));
                if (rc == -1)
                    throw Exception::ErrnoError("fail to set authsize on AF_ALG socket");
            }

            m_FD = accept(sFD, NULL, 0);
            if (m_FD == -1)
                throw Exception::ErrnoError("fail to accept on AF_ALG socket");

            Util::Raii sInitError([this]() { ::close(m_FD); });

            char sBuf[CMSG_SPACE(4) + CMSG_SPACE(4 + aCfg.iv.size())];
            memset(sBuf, 0, sizeof(sBuf));

            struct iovec sIov;
            sIov.iov_base = 0;
            sIov.iov_len = 0;

            struct msghdr sMsg;
            memset(&sMsg, 0, sizeof(sMsg));
            sMsg.msg_iov = &sIov;
            sMsg.msg_iovlen = 1;
            sMsg.msg_control = sBuf;
            sMsg.msg_controllen = sizeof(sBuf);

            struct cmsghdr* sCmsg = CMSG_FIRSTHDR(&sMsg);
            assert(sCmsg);
            sCmsg->cmsg_level          = SOL_ALG;
            sCmsg->cmsg_type           = ALG_SET_OP;
            sCmsg->cmsg_len            = CMSG_LEN(4);
            *(__u32 *)CMSG_DATA(sCmsg) = aCfg.encrypt ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT;

            sCmsg             = CMSG_NXTHDR(&sMsg, sCmsg);
            sCmsg->cmsg_level = SOL_ALG;
            sCmsg->cmsg_type  = ALG_SET_IV;
            sCmsg->cmsg_len   = CMSG_LEN(4 + aCfg.iv.size());
            af_alg_iv* sIV    = (af_alg_iv *)CMSG_DATA(sCmsg);
            sIV->ivlen        = aCfg.iv.size();
            memcpy(sIV->iv, aCfg.iv.data(), aCfg.iv.size());

            rc = sendmsg(m_FD, &sMsg, MSG_MORE);
            if (rc == -1)
                throw Exception::ErrnoError("fail to setup iv on AF_ALG socket");
            assert(rc == 0);

            sInitError.dismiss();
        }

        template<class T>
        std::string operator()(const T& aData)
        {
            if (m_TagSize and !m_Encrypt and (int)aData.size() < m_TagSize)
                throw std::runtime_error("fail to process: input too short");

            int rc = send(m_FD, aData.data(), aData.size(), 0);   // chunk done
            if (rc != (int)aData.size())
                throw Exception::ErrnoError("fail to feed AF_ALG socket");

            std::string sResult(aData.size() + (m_Encrypt ? m_TagSize : -m_TagSize), '\0');

            rc = read(m_FD, &sResult[0], sResult.size());
            if (rc != (int)sResult.size())
                throw Exception::ErrnoError("fail to read from AF_ALG socket");

            return sResult;
        }

        ~CryptoImpl() throw () { ::close(m_FD); }
    };
}