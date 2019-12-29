#pragma once

#include "CoreSocket.hpp"

namespace Udp
{
    class Socket : public Util::CoreSocket
    {
        struct sockaddr_in m_Peer;

        void create()
        {
            this->m_Fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (this->m_Fd == -1) { throw Error("fail to create a socket"); }
            memset(&m_Peer, 0, sizeof(m_Peer));
        }
    public:

        Socket(uint16_t aPort = 0, bool aReuse = false) // `server` socket
        {
            create();
            if (aReuse)
                this->set_reuse_port();
            bind(aPort);
        }

        Socket(uint32_t aRemote, uint16_t aPort)    // `connect` to remote
        {
            create();
            this->bind();
            m_Peer.sin_family = AF_INET;
	        m_Peer.sin_port = htons(aPort);
	        m_Peer.sin_addr.s_addr = aRemote;
        }

        struct Msg
        {
            ssize_t size = 0;
            std::string message;
            struct sockaddr_in addr;

            // remote in network byte order. port - in host order
            void set_peer(uint32_t aRemote, uint16_t aPort)
            {
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
	            addr.sin_port = htons(aPort);
	            addr.sin_addr.s_addr = aRemote;
            }
        };

        Msg read()
        {
            Msg sResult;
            auto sSize = ionread();
            if (sSize > 0)
            {
                socklen_t sLen = sizeof(sResult.addr);
                sResult.message.resize(sSize);
                sResult.size = checkCall([&](){ return ::recvfrom(m_Fd, sResult.message.data(), sSize, MSG_TRUNC, (struct sockaddr *)&sResult.addr, &sLen); }, "recv");
            }
            return sResult;
        }

        ssize_t read(void* aPtr, ssize_t aSize)
        {
            struct sockaddr_in sAddr;
            socklen_t sLen = sizeof(sAddr);
            return checkCall([&](){ return ::recvfrom(m_Fd, aPtr, aSize, MSG_TRUNC, (struct sockaddr *)&sAddr, &sLen); }, "recv");
        }

        // aCount - max number of messages to read
        // returns the number of messages received
        ssize_t read(struct mmsghdr* aPtr, ssize_t aCount)
        {
            // timeout is bugged, so just use nonblocking socket
            return recvmmsg(m_Fd, aPtr, aCount, MSG_TRUNC, nullptr);
        }

        ssize_t write(const void* aPtr, ssize_t aSize)
        {
            socklen_t sLen = sizeof(m_Peer);
            return checkCall([&](){ return ::sendto(m_Fd, aPtr, aSize, 0, (struct sockaddr *)&m_Peer, sLen); }, "send");
        }

        ssize_t write(const Msg& aMessage)
        {
            socklen_t sLen = sizeof(aMessage.addr);
            return checkCall([&](){ return ::sendto(m_Fd, aMessage.message.data(), aMessage.message.size(), 0, (struct sockaddr *)&aMessage.addr, sLen); }, "send");
        }
    };
}